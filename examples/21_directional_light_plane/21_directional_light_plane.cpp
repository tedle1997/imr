#include "imr/imr.h"
#include "imr/util.h"

#include <cmath>
#include "nasl/nasl.h"
#include "nasl/nasl_mat.h"

#include "../common/camera.h"

using namespace nasl;

struct Tri { 
    vec3 v0, v1, v2; 
    vec3 color; 
    vec3 normal;
};

struct Cube {
    Tri triangles[12];
};

struct Plane {
    Tri triangles[2];
};

// Calculate triangle normal from vertices
vec3 calculate_normal(vec3 v0, vec3 v1, vec3 v2) {
    vec3 edge1 = v1 - v0;
    vec3 edge2 = v2 - v0;
    vec3 normal = cross(edge1, edge2);
    return normalize(normal);
}

Cube make_cube() {
    /*
     *  +Y
     *  ^
     *  |
     *  |
     *  D------C.
     *  |\     |\
     *  | H----+-G
     *  | |    | |
     *  A-+----B | ---> +X
     *   \|     \|
     *    E------F
     *     \
     *      \
     *       \
     *        v +Z
     */
    vec3 A = { 0, 0, 0 };
    vec3 B = { 1, 0, 0 };
    vec3 C = { 1, 1, 0 };
    vec3 D = { 0, 1, 0 };
    vec3 E = { 0, 0, 1 };
    vec3 F = { 1, 0, 1 };
    vec3 G = { 1, 1, 1 };
    vec3 H = { 0, 1, 1 };

    int i = 0;
    Cube cube = {};

    auto add_face = [&](vec3 v0, vec3 v1, vec3 v2, vec3 v3, vec3 color) {
        /*
         * v0 --- v3
         *  |   / |
         *  |  /  |
         *  | /   |
         * v1 --- v2
         */
        vec3 normal1 = calculate_normal(v0, v1, v3);
        vec3 normal2 = calculate_normal(v1, v2, v3);
        
        cube.triangles[i++] = { v0, v1, v3, color, normal1 };
        cube.triangles[i++] = { v1, v2, v3, color, normal2 };
    };

    // top face (positive Y)
    add_face(H, D, C, G, vec3(0.3, 0.3, 0.9));
    // north face (negative Z)
    add_face(A, B, C, D, vec3(0.2, 0.2, 0.8));
    // west face (negative X)
    add_face(A, D, H, E, vec3(0.1, 0.1, 0.7));
    // east face (positive X)
    add_face(F, G, C, B, vec3(0.1, 0.1, 0.7));
    // south face (positive Z)
    add_face(E, H, G, F, vec3(0.0, 0.0, 0.6));
    // bottom face (negative Y)
    add_face(E, F, B, A, vec3(0.0, 0.0, 0.5));
    
    assert(i == 12);
    return cube;
}

Plane make_plane() {
    // Create a large plane at Y=0
    vec3 A = { -5, 0, -5 };
    vec3 B = {  5, 0, -5 };
    vec3 C = {  5, 0,  5 };
    vec3 D = { -5, 0,  5 };
    
    vec3 plane_color = vec3(0.3, 0.6, 0.3); // Green ground
    
    Plane plane = {};
    
    // Triangle 1: A, B, C
    plane.triangles[0] = { 
        A, B, C, 
        plane_color, 
        calculate_normal(A, B, C)
    };
    
    // Triangle 2: A, C, D
    plane.triangles[1] = { 
        A, C, D, 
        plane_color, 
        calculate_normal(A, C, D)
    };
    
    return plane;
}

struct {
    Tri tri;
    mat4 matrix;
    vec3 light_direction;
    float time;
} push_constants;

Camera camera;
CameraFreelookState camera_state = {
    .fly_speed = 3.0f,
    .mouse_sensitivity = 1.0f,
};
CameraInput camera_input;

bool reload_shaders = false;

int main() {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    auto window = glfwCreateWindow(1024, 1024, "Directional Light Example", nullptr, nullptr);

    glfwSetKeyCallback(window, [](GLFWwindow* window, int key, int scancode, int action, int mods) {
        if (key == GLFW_KEY_R && (mods & GLFW_MOD_CONTROL))
            reload_shaders = true;
    });

    imr::Context context;
    imr::Device device(context);
    imr::Swapchain swapchain(device, window);
    imr::FpsCounter fps_counter;
    std::unique_ptr<imr::ComputePipeline> shader;
    shader = std::make_unique<imr::ComputePipeline>(device, "21_directional_light_plane.spv");

    auto cube = make_cube();
    auto plane = make_plane();

    auto prev_frame = imr_get_time_nano();
    float delta = 0;

    // Position camera to see both cube and plane
    camera = {{0, 3, 8}, {-0.3, 0}, 60};

    // Directional light simulating sun
    vec3 light_direction = normalize(vec3(1, -1, -1));

    std::unique_ptr<imr::Image> depthBuffer;

    auto& vk = device.dispatch;
    while (!glfwWindowShouldClose(window)) {
        fps_counter.tick();
        fps_counter.updateGlfwWindowTitle(window);

        swapchain.renderFrameSimplified([&](imr::Swapchain::SimplifiedRenderContext& context) {
            camera_update(window, &camera_input);
            camera_move_freelook(&camera, &camera_input, &camera_state, delta);

            if (reload_shaders) {
                swapchain.drain();
                shader = std::make_unique<imr::ComputePipeline>(device, "21_directional_light_plane.spv");
                reload_shaders = false;
            }

            auto& image = context.image();
            auto cmdbuf = context.cmdbuf();

            if (!depthBuffer || depthBuffer->size().width != context.image().size().width || depthBuffer->size().height != context.image().size().height) {
                VkImageUsageFlagBits depthBufferFlags = static_cast<VkImageUsageFlagBits>(VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_STORAGE_BIT);
                depthBuffer = std::make_unique<imr::Image>(device, VK_IMAGE_TYPE_2D, context.image().size(), VK_FORMAT_R32_SFLOAT, depthBufferFlags);

                vk.cmdPipelineBarrier2KHR(cmdbuf, tmpPtr((VkDependencyInfo) {
                    .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                    .dependencyFlags = 0,
                    .imageMemoryBarrierCount = 1,
                    .pImageMemoryBarriers = tmpPtr((VkImageMemoryBarrier2) {
                        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                        .srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                        .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                        .dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                        .dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                        .newLayout = VK_IMAGE_LAYOUT_GENERAL,
                        .image = depthBuffer->handle(),
                        .subresourceRange = depthBuffer->whole_image_subresource_range()
                    })
                }));
            }

            vk.cmdClearColorImage(cmdbuf, image.handle(), VK_IMAGE_LAYOUT_GENERAL, tmpPtr((VkClearColorValue) {
                .float32 = { 0.1f, 0.1f, 0.2f, 1.0f }, // Dark blue background
            }), 1, tmpPtr(image.whole_image_subresource_range()));

            vk.cmdClearColorImage(cmdbuf, depthBuffer->handle(), VK_IMAGE_LAYOUT_GENERAL, tmpPtr((VkClearColorValue) {
                .float32 = { 1.0f, 0.0f, 0.0f, 0.0f },
            }), 1, tmpPtr(depthBuffer->whole_image_subresource_range()));

            // Barrier to ensure clear is finished
            vk.cmdPipelineBarrier2KHR(cmdbuf, tmpPtr((VkDependencyInfo) {
                .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                .dependencyFlags = 0,
                .memoryBarrierCount = 1,
                .pMemoryBarriers = tmpPtr((VkMemoryBarrier2) {
                    .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
                    .srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                    .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                    .dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                    .dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                })
            }));

            vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_COMPUTE, shader->pipeline());
            auto shader_bind_helper = shader->create_bind_helper();
            shader_bind_helper->set_storage_image(0, 0, image);
            shader_bind_helper->set_storage_image(0, 1, *depthBuffer);
            shader_bind_helper->commit(cmdbuf);

            auto add_render_barrier = [&]() {
                vk.cmdPipelineBarrier2KHR(cmdbuf, tmpPtr((VkDependencyInfo) {
                   .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                   .dependencyFlags = 0,
                   .memoryBarrierCount = 1,
                   .pMemoryBarriers = tmpPtr((VkMemoryBarrier2) {
                       .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
                       .srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                       .srcAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT,
                       .dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                       .dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT,
                   })
               }));
            };

            // Setup transformation matrices
            mat4 m = identity_mat4;
            mat4 flip_y = identity_mat4;
            flip_y.rows[1][1] = -1;
            m = m * flip_y;
            mat4 view_mat = camera_get_view_mat4(&camera, context.image().size().width, context.image().size().height);
            m = m * view_mat;

            push_constants.time = ((imr_get_time_nano() / 1000) % 10000000000) / 1000000.0f;
            push_constants.light_direction = light_direction;

            // Render plane triangles
            mat4 plane_matrix = m;
            for (int i = 0; i < 2; i++) {
                add_render_barrier();

                push_constants.tri = plane.triangles[i];
                push_constants.matrix = plane_matrix;

                vkCmdPushConstants(cmdbuf, shader->layout(), VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push_constants), &push_constants);
                vkCmdDispatch(cmdbuf, (image.size().width + 31) / 32, (image.size().height + 31) / 32, 1);
            }

            // Render cube triangles - position cube above the plane
            mat4 cube_matrix = m;
            cube_matrix = cube_matrix * translate_mat4(vec3(0, 1, 0)); // Lift cube 1 unit above plane
            cube_matrix = cube_matrix * translate_mat4(vec3(-0.5, -0.5, -0.5)); // Center cube

            for (int i = 0; i < 12; i++) {
                add_render_barrier();

                push_constants.tri = cube.triangles[i];
                push_constants.matrix = cube_matrix;

                vkCmdPushConstants(cmdbuf, shader->layout(), VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push_constants), &push_constants);
                vkCmdDispatch(cmdbuf, (image.size().width + 31) / 32, (image.size().height + 31) / 32, 1);
            }

            context.addCleanupAction([=, &device]() {
                delete shader_bind_helper;
            });

            auto now = imr_get_time_nano();
            delta = ((float) ((now - prev_frame) / 1000L)) / 1000000.0f;
            prev_frame = now;

            glfwPollEvents();
        });
    }

    swapchain.drain();
    return 0;
}
