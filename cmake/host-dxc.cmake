function(lod_setup_host_dxc)
    if (NOT CMAKE_CROSSCOMPILING)
        return()
    endif()

    set(_root "${CMAKE_SOURCE_DIR}/lib/rt64/src/contrib/dxc")

    if (CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64|AMD64)$")
        set(_arch "x64")
    elseif (CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "^(arm64|aarch64|ARM64)$")
        set(_arch "arm64")
    else()
        message(FATAL_ERROR
            "No prebuilt DXC for host processor '${CMAKE_HOST_SYSTEM_PROCESSOR}'.")
    endif()

    if (CMAKE_HOST_WIN32)
        set(_dxc "${_root}/bin/x64/dxc.exe")
    elseif (CMAKE_HOST_APPLE)
        set(_dxc "DYLD_LIBRARY_PATH=${_root}/lib/${_arch}" "${_root}/bin/${_arch}/dxc-macos")
    else()
        set(_dxc "LD_LIBRARY_PATH=${_root}/lib/${_arch}" "${_root}/bin/${_arch}/dxc-linux")
    endif()

    list(GET _dxc -1 _exe)
    if (NOT EXISTS "${_exe}")
        message(FATAL_ERROR "Host DXC binary was not found at '${_exe}'.")
    endif()

    set(RT64_HOST_DXC "${_dxc}" CACHE INTERNAL
        "DXC command line that runs on the build machine")
    message(STATUS "Host DXC: ${_exe}")
endfunction()
