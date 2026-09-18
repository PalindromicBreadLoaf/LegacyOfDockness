cmake_minimum_required(VERSION 3.20)
project(lod_host_tools LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (NOT LOD_RT64_DIR)
    message(FATAL_ERROR "LOD_RT64_DIR must point at the rt64 source tree")
endif()
if (NOT EXISTS "${LOD_RT64_DIR}/src/tools/file_to_c/file_to_c.cpp")
    message(FATAL_ERROR
        "rt64 sources not found under '${LOD_RT64_DIR}'.\n"
        "Did you forget 'git submodule update --init --recursive'?")
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
foreach (_cfg IN ITEMS DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${_cfg} "${CMAKE_BINARY_DIR}/bin")
endforeach()
unset(_cfg)

set(ZSTD_LEGACY_SUPPORT OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_STATIC   ON  CACHE BOOL "" FORCE)
set(ZSTD_BUILD_SHARED   OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_CONTRIB  OFF CACHE BOOL "" FORCE)
add_subdirectory("${LOD_RT64_DIR}/src/contrib/zstd/build/cmake" zstd)

add_compile_definitions(HLSL_CPU)
include_directories(
    "${LOD_RT64_DIR}/src"
    "${LOD_RT64_DIR}/src/contrib"
    "${LOD_RT64_DIR}/src/contrib/hlslpp/include"
)

add_subdirectory("${LOD_RT64_DIR}/src/tools/file_to_c"       file_to_c)
add_subdirectory("${LOD_RT64_DIR}/src/tools/texture_hasher"  texture_hasher)
add_subdirectory("${LOD_RT64_DIR}/src/tools/texture_packer"  texture_packer)
