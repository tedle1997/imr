# add_executable(22_shadow_mapping 22_shadow_mapping.cpp ../common/camera.cpp)
add_executable(22_shadow_mapping 22_shadow_mapping.cpp ../common/camera.cpp)

target_link_libraries(22_shadow_mapping imr nasl::nasl)

add_custom_target(22_shadow_mapping_spv COMMAND ${GLSLANG_EXE} -V -S comp ${CMAKE_CURRENT_SOURCE_DIR}/22_shadow_mapping.glsl -o ${CMAKE_CURRENT_BINARY_DIR}/22_shadow_mapping.spv)
add_dependencies(22_shadow_mapping 22_shadow_mapping_spv)
