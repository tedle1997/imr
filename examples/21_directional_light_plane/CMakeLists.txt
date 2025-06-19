add_executable(21_directional_light_plane 21_directional_light_plane.cpp ../common/camera.cpp)
target_link_libraries(21_directional_light_plane imr nasl::nasl)

add_custom_target(21_directional_light_plane_spv COMMAND ${GLSLANG_EXE} -V -S comp ${CMAKE_CURRENT_SOURCE_DIR}/21_directional_light_plane.glsl -o ${CMAKE_CURRENT_BINARY_DIR}/21_directional_light_plane.spv)
add_dependencies(21_directional_light_plane 21_directional_light_plane_spv)
