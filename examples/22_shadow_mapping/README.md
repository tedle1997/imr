# Example 22: Shadow Mapping

This example demonstrates **shadow mapping**, a fundamental 3D rendering technique for creating realistic shadows in real-time graphics applications. The implementation uses Vulkan compute shaders to render a scene with dynamic lighting and interactive shadow control.

## What This Example Shows

- **Shadow Map Generation**: Creates depth maps from the light's perspective
- **Multi-Pass Rendering**: Implements a three-pass rendering pipeline
- **Interactive Light Control**: Real-time adjustment of light direction using keyboard input
- **Directional Lighting**: Simulates sunlight or other distant light sources
- **Perspective-Correct Rendering**: Proper 3D projection and depth testing

## Scene Description

The rendered scene consists of:
- **A colored cube** positioned above a green ground plane
- **A ground plane** that receives shadows from the cube
- **A skybox** providing background environment
- **A red light ray visualization** showing the current light direction
- **Dynamic shadows** that update as you move the light

## Controls

### Light Direction Controls
- **Arrow Keys**: Adjust light direction
  - `←/→`: Rotate light azimuth (horizontal rotation)
  - `↑/↓`: Adjust light elevation (vertical angle)
- **Shift + Arrow Keys**: Fine control (smaller angle increments)

### Camera Controls
- **Mouse**: Look around (first-person camera)
- **WASD**: Move camera position
- **Scroll**: Adjust movement speed

### Shader Reloading
- **Ctrl+R**: Reload shaders without restarting

## Technical Implementation

### Multi-Pass Rendering Pipeline

1. **Shadow Map Pass** (`render_mode = 0`)
   - Renders scene from light's perspective
   - Generates 1024×1024 depth buffer (shadow map)
   - Only stores closest depth values

2. **Skybox Pass** (`render_mode = 2`)
   - Renders background environment
   - Uses camera rotation without translation
   - Rendered at maximum depth

3. **Main Scene Pass** (`render_mode = 1`)
   - Renders final scene with lighting and shadows
   - Samples shadow map to determine shadow visibility
   - Applies Lambert diffuse lighting model

### Shadow Mapping Algorithm

The shadow mapping technique works by:

1. **Light Space Transformation**: Converting world positions to light's coordinate system
2. **Depth Comparison**: Comparing current fragment depth with stored shadow map depth
3. **Shadow Factor Calculation**: Determining if a fragment is in shadow
4. **Bias Application**: Adding small offset to prevent shadow acne artifacts

### Lighting Model

- **Diffuse Lighting**: Lambert cosine law (`max(0, dot(normal, lightDir))`)
- **Ambient Lighting**: Constant 10% base illumination
- **Shadow Attenuation**: Reduces lighting to 30% in shadowed areas

### Mathematical Components

- **Barycentric Coordinates**: For triangle interpolation
- **Orthographic Projection**: For directional light shadow mapping
- **Spherical to Cartesian Conversion**: For intuitive light direction control
- **Perspective-Correct Interpolation**: For accurate 3D rendering

## Building and Running

### Prerequisites
- Vulkan-capable graphics hardware
- CMake build system
- GLSL shader compiler (glslang)

### Build Instructions

```bash
# From the project root directory
cd build
cmake ..
ninja
make 22_shadow_mapping

# Run the example
./examples/22_shadow_mapping/22_shadow_mapping
```

### Runtime Requirements
- The shader file `22_shadow_mapping.spv` must be in the working directory
- This is automatically generated during the build process

## Code Structure

### Main Components

- **Geometry Generation**: 
  - `make_cube()`: Creates a colored cube with proper normals
  - `make_plane()`: Generates ground plane for shadow reception
  - `make_skybox()`: Creates background environment
  - `make_light_ray_line()`: Visualizes light direction

- **Light Management**:
  - `spherical_to_cartesian()`: Converts spherical coordinates to direction vector
  - `create_light_view_proj_matrix()`: Sets up light's view-projection matrix
  - Interactive controls for azimuth and elevation

- **Rendering Pipeline**:
  - Three distinct render modes handled in compute shader
  - Depth testing and shadow map sampling
  - Multi-target rendering (color + depth + shadow map)

### Shader Architecture

The compute shader (`22_shadow_mapping.glsl`) handles:
- **Triangle rasterization** using barycentric coordinates
- **Depth testing** for proper occlusion
- **Shadow map generation** and sampling
- **Lighting calculations** with shadow attenuation

## Educational Value

This example teaches:

1. **Shadow Mapping Fundamentals**: Understanding how shadows are generated and applied
2. **Multi-Pass Rendering**: Organizing complex rendering pipelines
3. **3D Mathematics**: Practical application of linear algebra in graphics
4. **Real-Time Interaction**: Creating responsive 3D applications
5. **Vulkan Compute Shaders**: Using compute for graphics rendering

## Performance Considerations

- **Shadow Map Resolution**: 1024×1024 provides good quality/performance balance
- **Compute Shader Efficiency**: Uses 32×32 work groups for optimal GPU utilization
- **Memory Access Patterns**: Optimized image loads and stores
- **Conditional Rendering**: Early exits for improved performance

## Potential Enhancements

- **Percentage-Closer Filtering (PCF)**: Softer shadow edges
- **Cascade Shadow Maps**: Better shadow quality at different distances
- **Multiple Light Sources**: Additional shadow-casting lights
- **Shadow Map Bias Adjustment**: Runtime tweaking of shadow acne prevention
- **Performance Profiling**: GPU timing and optimization metrics

## Related Graphics Concepts

- **Depth Buffering**: Z-buffer algorithm for hidden surface removal
- **Texture Sampling**: Reading from depth textures
- **Coordinate System Transformations**: Moving between different 3D spaces
- **Real-Time Rendering**: Interactive frame rate considerations
- **GPU Programming**: Parallel processing for graphics

This example provides a solid foundation for understanding modern real-time shadow rendering techniques and can serve as a starting point for more advanced lighting and shadowing implementations.
