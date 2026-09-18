set(LOD_HOST_TOOLS "file_to_c;texture_hasher;texture_packer" CACHE INTERNAL
    "rt64 tools that must run on the build machine")

set(LOD_HOST_C_COMPILER "" CACHE FILEPATH
    "C compiler for the host-tool build")
set(LOD_HOST_CXX_COMPILER "" CACHE FILEPATH
    "C++ compiler for the host-tool build")

function(lod_setup_host_tools)
    if (NOT CMAKE_CROSSCOMPILING)
        return()
    endif()

    set(_src "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/host-tools")
    set(_bin "${CMAKE_BINARY_DIR}/host-tools")
    if (CMAKE_HOST_WIN32)
        set(_sfx ".exe")
    else()
        set(_sfx "")
    endif()

    set(_env "${CMAKE_COMMAND}" -E env
        --unset=CC --unset=CXX --unset=ASM
        --unset=CFLAGS --unset=CXXFLAGS --unset=LDFLAGS)

    if (NOT EXISTS "${_bin}/CMakeCache.txt")
        message(STATUS "Configuring host tools (${LOD_HOST_TOOLS}) for the build machine")
        set(_cfg_cmd ${_env} "${CMAKE_COMMAND}" -S "${_src}" -B "${_bin}"
            -G "${CMAKE_GENERATOR}"
            "-DLOD_RT64_DIR=${CMAKE_SOURCE_DIR}/lib/rt64"
            -DCMAKE_BUILD_TYPE=Release)
        if (CMAKE_MAKE_PROGRAM)
            list(APPEND _cfg_cmd "-DCMAKE_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}")
        endif()
        if (LOD_HOST_C_COMPILER)
            list(APPEND _cfg_cmd "-DCMAKE_C_COMPILER=${LOD_HOST_C_COMPILER}")
        endif()
        if (LOD_HOST_CXX_COMPILER)
            list(APPEND _cfg_cmd "-DCMAKE_CXX_COMPILER=${LOD_HOST_CXX_COMPILER}")
        endif()
        execute_process(COMMAND ${_cfg_cmd}
            RESULT_VARIABLE _res OUTPUT_VARIABLE _out ERROR_VARIABLE _out)
        if (NOT _res EQUAL 0)
            file(REMOVE_RECURSE "${_bin}")
            message(FATAL_ERROR "Host tool configure failed:\n${_out}")
        endif()
    endif()

    execute_process(
        COMMAND ${_env} "${CMAKE_COMMAND}" --build "${_bin}" --config Release
        RESULT_VARIABLE _res OUTPUT_VARIABLE _out ERROR_VARIABLE _out)
    if (NOT _res EQUAL 0)
        message(FATAL_ERROR "Host tool build failed:\n${_out}")
    endif()

    foreach (_tool IN LISTS LOD_HOST_TOOLS)
        set(_exe "${_bin}/bin/${_tool}${_sfx}")
        if (NOT EXISTS "${_exe}")
            message(FATAL_ERROR
                "Host tool '${_tool}' was not produced at '${_exe}'.\n"
                "Build output:\n${_out}")
        endif()
        add_executable(${_tool} IMPORTED GLOBAL)
        set_target_properties(${_tool} PROPERTIES IMPORTED_LOCATION "${_exe}")
        message(STATUS "Host tool ${_tool}: ${_exe}")
    endforeach()
endfunction()
