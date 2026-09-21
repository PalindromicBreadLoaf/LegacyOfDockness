include(${CMAKE_SOURCE_DIR}/cmake/switch-link.cmake)

set(PLUME_TRIANGLE_SHADERS
    ${CMAKE_SOURCE_DIR}/lib/rt64/src/contrib/plume/examples/triangle/shaders)

add_executable(switch_triangle ${CMAKE_CURRENT_SOURCE_DIR}/triangle.cpp)

function(switch_triangle_shader HLSL BLOB_NAME ENTRY PROFILE)
    set(_out ${CMAKE_CURRENT_BINARY_DIR}/shaders/${BLOB_NAME}.hlsl)
    add_custom_command(OUTPUT ${_out}.spv
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/shaders
        COMMAND ${RT64_HOST_DXC} -spirv -fspv-target-env=vulkan1.0 -fvk-use-dx-layout
            -E ${ENTRY} -T ${PROFILE} ${PLUME_TRIANGLE_SHADERS}/${HLSL} /Fo ${_out}.spv
        DEPENDS ${PLUME_TRIANGLE_SHADERS}/${HLSL})
    add_custom_command(OUTPUT ${_out}.spirv.c
        COMMAND file_to_c ${_out}.spv ${BLOB_NAME}BlobSPIRV ${_out}.spirv.c ${_out}.spirv.h
        DEPENDS ${_out}.spv file_to_c
        BYPRODUCTS ${_out}.spirv.h)
    target_sources(switch_triangle PRIVATE ${_out}.spirv.c)
endfunction()

switch_triangle_shader(triangle.vert.hlsl triangleVert VSMain vs_6_3)
switch_triangle_shader(triangle.frag.hlsl triangleFrag PSMain ps_6_3)

target_include_directories(switch_triangle PRIVATE
    ${CMAKE_SOURCE_DIR}/lib/rt64/src/contrib/plume
    ${CMAKE_CURRENT_BINARY_DIR}
)

target_link_libraries(switch_triangle PRIVATE plume)

lod_switch_add_link_support(switch_triangle)
lod_switch_link_nxvk(switch_triangle)

nx_generate_nacp(switch_triangle.nacp
    NAME "LodRecomp Switch Triangle"
    AUTHOR "PalindromicBreadLoaf"
    VERSION "${LOD_RECOMP_VERSION}"
)
nx_create_nro(switch_triangle NACP switch_triangle.nacp)
