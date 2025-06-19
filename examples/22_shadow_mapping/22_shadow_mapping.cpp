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

struct SkyboxTri {
    vec3 v0, v1, v2; 
};

struct Skybox {
    SkyboxTri triangles[12]; // 6 faces * 2 triangles each
};

struct Line {
    Tri triangles[2]; // Two triangles to form a thin quad for the line
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
    
    // Triangle 1: A, B, C (counter-clockwise from above for upward normal)
    plane.triangles[0] = { 
        A, B, C, 
        plane_color, 
        calculate_normal(A, B, C)
    };
    
    // Triangle 2: A, C, D (counter-clockwise from above for upward normal)
    plane.triangles[1] = { 
        A, C, D, 
        plane_color, 
        calculate_normal(A, C, D)
    };
    
    return plane;
}

Skybox make_skybox() {
    // Create a large cube centered at origin for skybox
    float size = 50.0f;
    vec3 A = { -size, -size, -size };
    vec3 B = {  size, -size, -size };
    vec3 C = {  size,  size, -size };
    vec3 D = { -size,  size, -size };
    vec3 E = { -size, -size,  size };
    vec3 F = {  size, -size,  size };
    vec3 G = {  size,  size,  size };
    vec3 H = { -size,  size,  size };

    Skybox skybox = {};
    int i = 0;

    auto add_face = [&](vec3 v0, vec3 v1, vec3 v2, vec3 v3) {
        // Note: skybox faces should be wound inward (opposite to normal geometry)
        skybox.triangles[i++] = { v0, v3, v1 };
        skybox.triangles[i++] = { v1, v3, v2 };
    };

    // top face (positive Y)
    add_face(H, D, C, G);
    // north face (negative Z)
    add_face(A, B, C, D);
    // west face (negative X)
    add_face(A, D, H, E);
    // east face (positive X)
    add_face(F, G, C, B);
    // south face (positive Z)
    add_face(E, H, G, F);
    // bottom face (negative Y)
    add_face(E, F, B, A);
    
    assert(i == 12);
    return skybox;
}

Line make_light_ray_line(vec3 cube_center, vec3 light_direction, float length) {
    // Create a thin line from cube center extending in light direction
    vec3 start = cube_center;
    vec3 end = start + light_direction * length;
    
    // Create a thin quad to represent the line
    vec3 line_vec = normalize(end - start);
    vec3 perp1 = vec3(0, 1, 0);
    if (abs(dot(line_vec, perp1)) > 0.9f) {
        perp1 = vec3(1, 0, 0);
    }
    vec3 perp2 = normalize(cross(line_vec, perp1));
    perp1 = normalize(cross(perp2, line_vec));
    
    float line_thickness = 0.02f;
    vec3 offset1 = perp1 * line_thickness;
    vec3 offset2 = perp2 * line_thickness;
    
    // Create 4 vertices for the line quad
    vec3 v0 = start - offset1;
    vec3 v1 = start + offset1;
    vec3 v2 = end + offset1;
    vec3 v3 = end - offset1;
    
    Line line = {};
    vec3 red_color = vec3(1.0, 0.0, 0.0); // Red color
    
    // Triangle 1: v0, v1, v2
    line.triangles[0] = { 
        v0, v1, v2, 
        red_color, 
        calculate_normal(v0, v1, v2)
    };
    
    // Triangle 2: v0, v2, v3
    line.triangles[1] = { 
        v0, v2, v3, 
        red_color, 
        calculate_normal(v0, v2, v3)
    };
    
    return line;
}

struct {
    Tri tri;
    mat4 matrix;
    vec3 light_direction;
    float time;
    mat4 light_view_proj_matrix;
    int render_mode; // 0 = shadow map, 1 = final render, 2 = skybox
    int apply_shadows; // 0 = no shadows, 1 = apply shadows
} push_constants;

Camera camera;
CameraFreelookState camera_state = {
    .fly_speed = 3.0f,
    .mouse_sensitivity = 1.0f,
};
CameraInput camera_input;

bool reload_shaders = false;

// Light control variables
float light_azimuth = 45.0f;    // Azimuthal angle in degrees (0-360)
float light_elevation = 45.0f;  // Elevation angle in degrees (1-89)
const float ANGLE_STEP = 2.0f;  // Degrees per key press (normal speed)
const float FINE_ANGLE_STEP = 0.5f;  // Degrees per key press (fine control)

// Convert spherical coordinates to cartesian direction vector
vec3 spherical_to_cartesian(float azimuth_deg, float elevation_deg) {
    float azimuth_rad = azimuth_deg * M_PI / 180.0f;
    float elevation_rad = elevation_deg * M_PI / 180.0f;
    
    // Standard spherical coordinates:
    // x = sin(elevation) * cos(azimuth)
    // y = cos(elevation) (pointing down when elevation=0, up when elevation=180)
    // z = sin(elevation) * sin(azimuth)
    float x = sin(elevation_rad) * cos(azimuth_rad);
    float y = cos(elevation_rad); // Positive to point upward (light from above)
    float z = sin(elevation_rad) * sin(azimuth_rad);
    
    return normalize(vec3(x, y, z));
}

// Create orthographic projection matrix
mat4 ortho_matrix(float left, float right, float bottom, float top, float near, float far) {
    mat4 m = identity_mat4;
    m.elems.m00 = 2.0f / (right - left);
    m.elems.m11 = 2.0f / (top - bottom);
    m.elems.m22 = -2.0f / (far - near);
    m.elems.m30 = -(right + left) / (right - left);
    m.elems.m31 = -(top + bottom) / (top - bottom);
    m.elems.m32 = -(far + near) / (far - near);
    return m;
}

// Create look-at view matrix
mat4 look_at_matrix(vec3 eye, vec3 center, vec3 up) {
    vec3 f = normalize(center - eye);
    vec3 s = normalize(cross(f, up));
    vec3 u = cross(s, f);
    
    mat4 result = identity_mat4;
    result.elems.m00 = s.x;
    result.elems.m10 = s.y;
    result.elems.m20 = s.z;
    result.elems.m01 = u.x;
    result.elems.m11 = u.y;
    result.elems.m21 = u.z;
    result.elems.m02 = -f.x;
    result.elems.m12 = -f.y;
    result.elems.m22 = -f.z;
    result.elems.m30 = -dot(s, eye);
    result.elems.m31 = -dot(u, eye);
    result.elems.m32 = dot(f, eye);
    
    return result;
}

// Create light view-projection matrix for shadow mapping
mat4 create_light_view_proj_matrix(vec3 light_direction, vec3 scene_center, float scene_radius) {
    // Position light far away in the opposite direction of light_direction
    vec3 light_pos = scene_center - light_direction * scene_radius * 3.0f;
    
    // Create orthographic projection for directional light
    float light_coverage_size = scene_radius * 2.0f;
    mat4 light_proj = ortho_matrix(-light_coverage_size, light_coverage_size, -light_coverage_size, light_coverage_size, 0.1f, scene_radius * 6.0f);
    
    // Create view matrix looking from light position towards scene center
    vec3 up = vec3(0, 1, 0);
    if (abs(dot(light_direction, up)) > 0.9f) {
        up = vec3(1, 0, 0); // Use different up vector if light is nearly vertical
    }
    mat4 light_view = look_at_matrix(light_pos, scene_center, up);
    
    return light_proj * light_view;
}

int main() {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    auto window = glfwCreateWindow(1024, 1024, "Shadow Mapping Example", nullptr, nullptr);

    glfwSetKeyCallback(window, [](GLFWwindow* window, int key, int scancode, int action, int mods) {
        if (key == GLFW_KEY_R && (mods & GLFW_MOD_CONTROL))
            reload_shaders = true;
            
        // Light control keys
        if (action == GLFW_PRESS || action == GLFW_REPEAT) {
            switch (key) {
                case GLFW_KEY_LEFT:   // Decrease azimuth (rotate light counter-clockwise)
                    if (mods & GLFW_MOD_SHIFT) {
                        light_azimuth -= FINE_ANGLE_STEP; // Fine control with Shift
                    } else {
                        light_azimuth -= ANGLE_STEP; // Normal control
                    }
                    if (light_azimuth < 0.0f) light_azimuth += 360.0f;
                    break;
                case GLFW_KEY_RIGHT:  // Increase azimuth (rotate light clockwise)  
                    if (mods & GLFW_MOD_SHIFT) {
                        light_azimuth += FINE_ANGLE_STEP; // Fine control with Shift
                    } else {
                        light_azimuth += ANGLE_STEP; // Normal control
                    }
                    if (light_azimuth >= 360.0f) light_azimuth -= 360.0f;
                    break;
                case GLFW_KEY_UP:     // Increase elevation (light higher)
                    if (mods & GLFW_MOD_SHIFT) {
                        light_elevation += FINE_ANGLE_STEP; // Fine control with Shift
                    } else {
                        light_elevation += ANGLE_STEP; // Normal control
                    }
                    if (light_elevation > 89.0f) light_elevation = 89.0f; // Nearly straight down
                    break;
                case GLFW_KEY_DOWN:   // Decrease elevation (light lower)
                    if (mods & GLFW_MOD_SHIFT) {
                        light_elevation -= FINE_ANGLE_STEP; // Fine control with Shift
                    } else {
                        light_elevation -= ANGLE_STEP; // Normal control
                    }
                    if (light_elevation < 1.0f) light_elevation = 1.0f; // Nearly straight up
                    break;
            }
        }
    });

    imr::Context context;
    imr::Device device(context);
    imr::Swapchain swapchain(device, window);
    imr::FpsCounter fps_counter;
    std::unique_ptr<imr::ComputePipeline> shader;
    shader = std::make_unique<imr::ComputePipeline>(device, "22_shadow_mapping.spv");

    auto cube = make_cube();
    auto plane = make_plane();
    auto skybox = make_skybox();

    auto prev_frame = imr_get_time_nano();
    float delta = 0;

    // Position camera to see both cube and plane
    camera = {{0, 3, 8}, {-0.3, 0}, 60};

    std::unique_ptr<imr::Image> depthBuffer;
    std::unique_ptr<imr::Image> shadowMap;
    const int SHADOW_MAP_SIZE = 1024;

    auto& vk = device.dispatch;
    while (!glfwWindowShouldClose(window)) {
        fps_counter.tick();
        
        // Update window title with light controls
        char title[256];
        snprintf(title, sizeof(title), "Shadow Mapping - Azimuth: %.1f° Elevation: %.1f° (Arrow keys: angle, Shift+Arrow: fine)", 
                light_azimuth, light_elevation);
        glfwSetWindowTitle(window, title);

        swapchain.renderFrameSimplified([&](imr::Swapchain::SimplifiedRenderContext& context) {
            camera_update(window, &camera_input);
            camera_move_freelook(&camera, &camera_input, &camera_state, delta);

            if (reload_shaders) {
                swapchain.drain();
                shader = std::make_unique<imr::ComputePipeline>(device, "22_shadow_mapping.spv");
                reload_shaders = false;
            }

            auto& image = context.image();
            auto cmdbuf = context.cmdbuf();

            // Create depth buffer if needed
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

            // Create shadow map if needed
            if (!shadowMap) {
                VkImageUsageFlagBits shadowMapFlags = static_cast<VkImageUsageFlagBits>(VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_STORAGE_BIT);
                shadowMap = std::make_unique<imr::Image>(device, VK_IMAGE_TYPE_2D, 
                    VkExtent3D{SHADOW_MAP_SIZE, SHADOW_MAP_SIZE, 1}, VK_FORMAT_R32_SFLOAT, shadowMapFlags);

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
                        .image = shadowMap->handle(),
                        .subresourceRange = shadowMap->whole_image_subresource_range()
                    })
                }));
            }

            // Clear main render target and depth buffer
            vk.cmdClearColorImage(cmdbuf, image.handle(), VK_IMAGE_LAYOUT_GENERAL, tmpPtr((VkClearColorValue) {
                .float32 = { 0.1f, 0.1f, 0.2f, 1.0f }, // Dark blue background
            }), 1, tmpPtr(image.whole_image_subresource_range()));

            vk.cmdClearColorImage(cmdbuf, depthBuffer->handle(), VK_IMAGE_LAYOUT_GENERAL, tmpPtr((VkClearColorValue) {
                .float32 = { 1.0f, 0.0f, 0.0f, 0.0f },
            }), 1, tmpPtr(depthBuffer->whole_image_subresource_range()));

            // Clear shadow map
            vk.cmdClearColorImage(cmdbuf, shadowMap->handle(), VK_IMAGE_LAYOUT_GENERAL, tmpPtr((VkClearColorValue) {
                .float32 = { 1.0f, 0.0f, 0.0f, 0.0f },
            }), 1, tmpPtr(shadowMap->whole_image_subresource_range()));

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
            shader_bind_helper->set_storage_image(0, 2, *shadowMap);
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

            // Calculate light direction from spherical coordinates
            vec3 light_direction = -spherical_to_cartesian(light_azimuth, light_elevation);

            // Create light view-projection matrix
            vec3 scene_center = vec3(0, 0.5, 0);
            float scene_radius = 8.0f;
            mat4 light_view_proj = create_light_view_proj_matrix(light_direction, scene_center, scene_radius);

            // Create light ray line from cube center
            vec3 cube_center = vec3(0, 1, 0); // Cube is lifted 1 unit above plane
            Line light_ray = make_light_ray_line(cube_center, -light_direction, 10.0f);

            push_constants.time = ((imr_get_time_nano() / 1000) % 10000000000) / 1000000.0f;
            push_constants.light_direction = light_direction;
            push_constants.light_view_proj_matrix = light_view_proj;
            push_constants.apply_shadows = 1; // Enable shadows by default

            // PASS 1: Generate shadow map from light's perspective
            push_constants.render_mode = 0; // Shadow map mode

            // Render plane triangles to shadow map
            for (int i = 0; i < 2; i++) {
                add_render_barrier();

                push_constants.tri = plane.triangles[i];
                push_constants.matrix = light_view_proj;

                vkCmdPushConstants(cmdbuf, shader->layout(), VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push_constants), &push_constants);
                vkCmdDispatch(cmdbuf, (SHADOW_MAP_SIZE + 31) / 32, (SHADOW_MAP_SIZE + 31) / 32, 1);
            }

            // Render cube triangles to shadow map
            mat4 cube_light_matrix = light_view_proj;
            cube_light_matrix = cube_light_matrix * translate_mat4(vec3(0, 1, 0)); // Lift cube 1 unit above plane
            cube_light_matrix = cube_light_matrix * translate_mat4(vec3(-0.5, -0.5, -0.5)); // Center cube

            for (int i = 0; i < 12; i++) {
                add_render_barrier();

                push_constants.tri = cube.triangles[i];
                push_constants.matrix = cube_light_matrix;

                vkCmdPushConstants(cmdbuf, shader->layout(), VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push_constants), &push_constants);
                vkCmdDispatch(cmdbuf, (SHADOW_MAP_SIZE + 31) / 32, (SHADOW_MAP_SIZE + 31) / 32, 1);
            }

            add_render_barrier(); // Ensure shadow map is complete before main render

            // PASS 2: Render skybox
            push_constants.render_mode = 2; // Skybox mode
            
            // Setup camera matrices for skybox (without translation)
            mat4 skybox_view = identity_mat4;
            mat4 flip_y = identity_mat4;
            flip_y.rows[1][1] = -1;
            skybox_view = skybox_view * flip_y;
            
            // Create view matrix without translation for skybox
            Camera skybox_camera = camera;
            skybox_camera.position = vec3(0, 0, 0); // Remove translation
            mat4 skybox_view_mat = camera_get_view_mat4(&skybox_camera, context.image().size().width, context.image().size().height);
            skybox_view = skybox_view * skybox_view_mat;

            for (int i = 0; i < 12; i++) {
                add_render_barrier();

                // Convert skybox triangle to regular triangle for rendering
                Tri skybox_tri = {
                    skybox.triangles[i].v0,
                    skybox.triangles[i].v1,
                    skybox.triangles[i].v2,
                    vec3(1.0, 1.0, 1.0), // White color
                    vec3(0, 0, 1) // Dummy normal for skybox
                };
                push_constants.tri = skybox_tri;
                push_constants.matrix = skybox_view;

                vkCmdPushConstants(cmdbuf, shader->layout(), VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push_constants), &push_constants);
                vkCmdDispatch(cmdbuf, (image.size().width + 31) / 32, (image.size().height + 31) / 32, 1);
            }

            // PASS 3: Render scene with shadows
            push_constants.render_mode = 1; // Final render mode with shadows

            // Setup transformation matrices for main scene
            mat4 m = identity_mat4;
            flip_y = identity_mat4;
            flip_y.rows[1][1] = -1;
            m = m * flip_y;
            mat4 view_mat = camera_get_view_mat4(&camera, context.image().size().width, context.image().size().height);
            m = m * view_mat;

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

            // Render light ray line (red line from cube center in light direction)
            for (int i = 0; i < 2; i++) {
                add_render_barrier();

                push_constants.tri = light_ray.triangles[i];
                push_constants.matrix = m;

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
