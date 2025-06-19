#version 450
#extension GL_EXT_shader_image_load_formatted : require
#extension GL_EXT_scalar_block_layout : require

layout(set = 0, binding = 0)
uniform image2D renderTarget;

layout(set = 0, binding = 1)
uniform image2D depthBuffer;

layout(set = 0, binding = 2)
uniform image2D shadowMap;

layout(local_size_x = 32, local_size_y = 32, local_size_z = 1) in;

struct Tri { 
    vec3 v0, v1, v2; 
    vec3 color; 
    vec3 normal;
};

#define dvec3 vec3
#define dvec2 vec2
#define double float

layout(scalar, push_constant) uniform T {
    Tri triangle;
    mat4 m;
    vec3 light_direction;
    float time;
    mat4 light_view_proj_matrix;
    int render_mode;
    int apply_shadows; // 0 = no shadows, 1 = apply shadows
} push_constants;

// Barycentric coordinate calculation
vec3 barycentric(vec2 p, vec2 a, vec2 b, vec2 c) {
    vec2 v0 = c - a;
    vec2 v1 = b - a;
    vec2 v2 = p - a;
    
    float dot00 = dot(v0, v0);
    float dot01 = dot(v0, v1);
    float dot02 = dot(v0, v2);
    float dot11 = dot(v1, v1);
    float dot12 = dot(v1, v2);
    
    float inv_denom = 1.0 / (dot00 * dot11 - dot01 * dot01);
    float u = (dot11 * dot02 - dot01 * dot12) * inv_denom;
    float v = (dot00 * dot12 - dot01 * dot02) * inv_denom;
    
    return vec3(1.0 - u - v, v, u);
}

// Point-in-triangle test using barycentric coordinates
bool point_in_triangle(vec2 p, vec2 a, vec2 b, vec2 c) {
    vec3 bary = barycentric(p, a, b, c);
    return bary.x >= 0.0 && bary.y >= 0.0 && bary.z >= 0.0;
}

// Interpolate values using barycentric coordinates
float interpolate_float(vec3 bary, float v0, float v1, float v2) {
    return bary.x * v0 + bary.y * v1 + bary.z * v2;
}

vec3 interpolate_vec3(vec3 bary, vec3 v0, vec3 v1, vec3 v2) {
    return bary.x * v0 + bary.y * v1 + bary.z * v2;
}

// Sample shadow map with basic filtering
float sample_shadow_map(vec2 shadow_coord) {
    ivec2 shadow_size = imageSize(shadowMap);
    ivec2 coord = ivec2(shadow_coord * vec2(shadow_size));
    
    // Clamp to shadow map bounds
    if (coord.x < 0 || coord.y < 0 || coord.x >= shadow_size.x || coord.y >= shadow_size.y) {
        return 1.0; // No shadow outside shadow map
    }
    
    return imageLoad(shadowMap, coord).r;
}

double cross_2(dvec2 a, dvec2 b) {
    return cross(dvec3(a, 0), dvec3(b, 0)).z;
}

double barCoord(dvec2 a, dvec2 b, dvec2 point){
    dvec2 PA = point - a;
    dvec2 BA = b - a;
    return cross_2(PA, BA);
}

dvec3 barycentricTri2(dvec2 v0, dvec2 v1, dvec2 v2, dvec2 point) {
    double triangleArea = barCoord(v0.xy, v1.xy, v2.xy);

    double u = barCoord(v0.xy, v1.xy, point) / triangleArea;
    double v = barCoord(v1.xy, v2.xy, point) / triangleArea;

    return dvec3(u, v, triangleArea);
}

bool is_inside_edge(dvec2 e0, dvec2 e1, dvec2 p) {
    if (e1.x == e0.x)
        return (e1.x > p.x) ^^ (e0.y > e1.y);
    double a = (e1.y - e0.y) / (e1.x - e0.x);
    double b = e0.y + (0 - e0.x) * a;
    double ey = a * p.x + b;
    return (ey < p.y) ^^ (e0.x > e1.x);
}

void main() {
    ivec2 img_size;
    
    if (push_constants.render_mode == 0) {
        // Shadow map generation mode
        img_size = imageSize(shadowMap);
    } else {
        // Regular rendering or skybox mode
        img_size = imageSize(renderTarget);
    }
    
    if (gl_GlobalInvocationID.x >= img_size.x || gl_GlobalInvocationID.y >= img_size.y)
        return;

    dvec2 point = dvec2(gl_GlobalInvocationID.xy) / vec2(img_size);
    point = point * 2.0 - dvec2(1.0);

    vec4 os_v0 = vec4(push_constants.triangle.v0, 1);
    vec4 os_v1 = vec4(push_constants.triangle.v1, 1);
    vec4 os_v2 = vec4(push_constants.triangle.v2, 1);
    vec4 v0 = push_constants.m * os_v0;
    vec4 v1 = push_constants.m * os_v1;
    vec4 v2 = push_constants.m * os_v2;
    dvec2 ss_v0 = dvec2(v0.xy) / v0.w;
    dvec2 ss_v1 = dvec2(v1.xy) / v1.w;
    dvec2 ss_v2 = dvec2(v2.xy) / v2.w;

    // Check if point is inside triangle
    bool backface = ((is_inside_edge(ss_v1.xy, ss_v0.xy, point) ^^ (v0.w < 0) ^^ (v1.w < 0)) && 
                     (is_inside_edge(ss_v2.xy, ss_v1.xy, point) ^^ (v1.w < 0) ^^ (v2.w < 0)) && 
                     (is_inside_edge(ss_v0.xy, ss_v2.xy, point) ^^ (v2.w < 0) ^^ (v0.w < 0)));
    bool frontface = (is_inside_edge(ss_v0.xy, ss_v1.xy, point) ^^ (v0.w < 0) ^^ (v1.w < 0)) && 
                     (is_inside_edge(ss_v1.xy, ss_v2.xy, point) ^^ (v1.w < 0) ^^ (v2.w < 0)) && 
                     (is_inside_edge(ss_v2.xy, ss_v0.xy, point) ^^ (v2.w < 0) ^^ (v0.w < 0));
    
    // Skip backface culling for skybox
    if (push_constants.render_mode != 2 && !frontface && !backface)
        return;
    if (push_constants.render_mode == 2 && !frontface && !backface)
        return;

    // Calculate barycentric coordinates
    dvec3 baryResults = barycentricTri2(ss_v0.xy, ss_v1.xy, ss_v2.xy, point);
    double u = baryResults.x;
    double v = baryResults.y;
    double w = 1 - u - v;

    dvec3 v_ws = vec3(v0.w, v1.w, v2.w);
    dvec3 ss_v_coefs = vec3(v, w, u);
    dvec3 pc_v_coefs = ss_v_coefs / v_ws;
    double pc_v_coefs_sum = pc_v_coefs.x + pc_v_coefs.y + pc_v_coefs.z;
    pc_v_coefs = pc_v_coefs / pc_v_coefs_sum;

    float depth = float(dot(ss_v_coefs, vec3(v0.z / v0.w, v1.z / v1.w, v2.z / v2.w)));

    if (depth < 0)
        return;

    // Handle different render modes
    if (push_constants.render_mode == 0) {
        // Shadow map mode - only update if closer
        float current_depth = imageLoad(shadowMap, ivec2(gl_GlobalInvocationID.xy)).x;
        if (depth < current_depth)
            imageStore(shadowMap, ivec2(gl_GlobalInvocationID.xy), vec4(depth));
        return;
    } else if (push_constants.render_mode == 2) {
        // Skybox mode - render at far depth but still do depth test
        float prevDepth = imageLoad(depthBuffer, ivec2(gl_GlobalInvocationID.xy)).x;
        if (depth < prevDepth) {
            imageStore(depthBuffer, ivec2(gl_GlobalInvocationID.xy), vec4(depth));
            imageStore(renderTarget, ivec2(gl_GlobalInvocationID.xy), vec4(push_constants.triangle.color, 1.0));
        }
        return;
    } else {
        // Regular rendering mode
        float prevDepth = imageLoad(depthBuffer, ivec2(gl_GlobalInvocationID.xy)).x;
        if (depth < prevDepth)
            imageStore(depthBuffer, ivec2(gl_GlobalInvocationID.xy), vec4(depth));
        else
            return;
    }

    // Calculate world position for shadow mapping
    vec3 world_pos = interpolate_vec3(vec3(pc_v_coefs), push_constants.triangle.v0, push_constants.triangle.v1, push_constants.triangle.v2);

    // Calculate lighting
    vec3 normal = normalize(push_constants.triangle.normal);
    vec3 lightDir = normalize(push_constants.light_direction);
    
    // Lambert diffuse lighting
    float NdotL = max(0.0, dot(normal, lightDir));
    
    // Calculate shadow
    float shadow_factor = 1.0;
    if (push_constants.render_mode == 1 && push_constants.apply_shadows == 1) {
        // Transform world position to light's clip space
        vec4 light_clip_pos = push_constants.light_view_proj_matrix * vec4(world_pos, 1.0);
        vec3 light_ndc = light_clip_pos.xyz / light_clip_pos.w;
        
        // Convert to shadow map coordinates [0,1]
        vec2 shadow_coord = light_ndc.xy * 0.5 + 0.5;
        
        // Check if within shadow map bounds
        if (shadow_coord.x >= 0.0 && shadow_coord.x <= 1.0 && 
            shadow_coord.y >= 0.0 && shadow_coord.y <= 1.0) {
            
            ivec2 shadow_size = imageSize(shadowMap);
            ivec2 coord = ivec2(shadow_coord * vec2(shadow_size));
            
            if (coord.x >= 0 && coord.y >= 0 && coord.x < shadow_size.x && coord.y < shadow_size.y) {
                float shadow_depth = imageLoad(shadowMap, coord).r;
                float current_light_depth = light_ndc.z;
                
                // Add bias to prevent shadow acne
                float bias = 0.001;
                if (current_light_depth > shadow_depth + bias) {
                    shadow_factor = 0.3; // In shadow
                }
            }
        }
    }
    
    // Apply lighting and shadow
    vec3 baseColor = push_constants.triangle.color;
    vec3 litColor = baseColor * NdotL * shadow_factor;
    vec3 ambient = baseColor * 0.1;
    vec3 finalColor = litColor + ambient;
    
    vec4 pixelColor = vec4(finalColor, 1.0);

    imageStore(renderTarget, ivec2(gl_GlobalInvocationID.xy), pixelColor);
}
