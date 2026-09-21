include_guard(GLOBAL)

function(lod_switch_strip_sdl2_mesa_gl)
    if (NOT TARGET SDL2::SDL2)
        message(FATAL_ERROR "lod_switch_strip_sdl2_mesa_gl() requires find_package(SDL2) first")
    endif()

    get_target_property(_sdl2_target SDL2::SDL2 ALIASED_TARGET)
    if (NOT _sdl2_target)
        set(_sdl2_target SDL2::SDL2)
    endif()

    get_target_property(_sdl2_link ${_sdl2_target} INTERFACE_LINK_LIBRARIES)
    if (NOT _sdl2_link)
        return()
    endif()

    list(REMOVE_ITEM _sdl2_link EGL glapi drm_nouveau)
    set_target_properties(${_sdl2_target} PROPERTIES INTERFACE_LINK_LIBRARIES "${_sdl2_link}")
endfunction()

function(lod_switch_add_link_support target)
    target_sources(${target} PRIVATE
        ${CMAKE_SOURCE_DIR}/src/platform/switch/sdl2_egl_stub.c
        ${CMAKE_SOURCE_DIR}/src/platform/switch/nvk_compat.c
    )
endfunction()

function(lod_switch_link_nxvk target)
    execute_process(
        COMMAND ${PKG_CONFIG_EXECUTABLE} --libs nxvk
        OUTPUT_VARIABLE _nxvk_libs
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE _nxvk_result
        ERROR_QUIET)
    if (NOT _nxvk_result EQUAL 0)
        message(FATAL_ERROR
            "nxvk portlib not found. Install it from github.com/PalindromicBreadLoaf/nxvk "
            "(make gl && make install-gl) so that ${PKG_CONFIG_EXECUTABLE} --libs nxvk succeeds.")
    endif()

    separate_arguments(_nxvk_libs_list NATIVE_COMMAND "${_nxvk_libs}")
    target_link_options(${target} PRIVATE ${_nxvk_libs_list})

    execute_process(
        COMMAND ${PKG_CONFIG_EXECUTABLE} --cflags nxvk
        OUTPUT_VARIABLE _nxvk_cflags
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)
    separate_arguments(_nxvk_cflags_list NATIVE_COMMAND "${_nxvk_cflags}")
    target_compile_options(${target} PRIVATE ${_nxvk_cflags_list})
endfunction()
