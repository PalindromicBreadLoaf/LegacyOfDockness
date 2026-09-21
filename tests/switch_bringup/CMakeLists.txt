include(${CMAKE_SOURCE_DIR}/cmake/switch-link.cmake)

add_executable(switch_bringup ${CMAKE_CURRENT_SOURCE_DIR}/bringup.cpp)

target_include_directories(switch_bringup PRIVATE
    ${SDL2_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/lib/N64ModernRuntime/librecomp/include
    ${CMAKE_SOURCE_DIR}/lib/N64ModernRuntime/ultramodern/include
    ${CMAKE_SOURCE_DIR}/lib/N64ModernRuntime/thirdparty
    ${CMAKE_SOURCE_DIR}/lib/N64ModernRuntime/N64Recomp/include
)

target_link_libraries(switch_bringup PRIVATE librecomp ultramodern SDL2::SDL2)

lod_switch_add_link_support(switch_bringup)
lod_switch_link_nxvk(switch_bringup)

nx_generate_nacp(switch_bringup.nacp
    NAME "LodRecomp Switch Bring-up"
    AUTHOR "PalindromicBreadLoaf"
    VERSION "${LOD_RECOMP_VERSION}"
)
nx_create_nro(switch_bringup
    NACP switch_bringup.nacp
    ROMFS ${CMAKE_CURRENT_SOURCE_DIR}/romfs
)
