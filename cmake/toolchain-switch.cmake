cmake_minimum_required(VERSION 3.20)

if (DEFINED ENV{DEVKITPRO})
    set(LOD_DEVKITPRO "$ENV{DEVKITPRO}")
else()
    set(LOD_DEVKITPRO "/opt/devkitpro")
endif()

if (NOT EXISTS "${LOD_DEVKITPRO}/cmake/Switch.cmake")
    message(FATAL_ERROR
        "devkitPro's Switch toolchain was not found at '${LOD_DEVKITPRO}/cmake/Switch.cmake'.\n"
        "Install devkitA64 + libnx (sudo dkp-pacman -S switch-dev) and export DEVKITPRO, e.g.\n"
        "  export DEVKITPRO=/opt/devkitpro")
endif()

if (NOT EXISTS "${LOD_DEVKITPRO}/libnx/include/switch.h")
    message(FATAL_ERROR
        "libnx was not found under '${LOD_DEVKITPRO}/libnx'. Install it with:\n"
        "  sudo dkp-pacman -S libnx")
endif()

set(LOD_SWITCH_DEVKITPRO "${LOD_DEVKITPRO}" CACHE INTERNAL
    "devkitPro root resolved by cmake/toolchain-switch.cmake")
set(CMAKE_USER_MAKE_RULES_OVERRIDE "${CMAKE_CURRENT_LIST_DIR}/switch-rule-overrides.cmake")

include("${LOD_DEVKITPRO}/cmake/Switch.cmake")

set(LOD_PLATFORM_SWITCH ON CACHE BOOL
    "Build LodRecomp for Nintendo Switch" FORCE)

set(LOD_SWITCH_PORTLIB_PKGCONFIG "${LOD_DEVKITPRO}/portlibs/switch/lib/pkgconfig")
set(LOD_SWITCH_PKG_nxvk      "nxvk (github.com/PalindromicBreadLoaf/nxvk)")
set(LOD_SWITCH_PKG_sdl2      "switch-sdl2")
set(LOD_SWITCH_PKG_zlib      "switch-zlib")
set(LOD_SWITCH_PKG_freetype2 "switch-freetype")
foreach (_lod_pkg IN ITEMS nxvk sdl2 zlib freetype2)
    if (NOT EXISTS "${LOD_SWITCH_PORTLIB_PKGCONFIG}/${_lod_pkg}.pc")
        message(WARNING "Switch portlib '${_lod_pkg}' not installed — ${LOD_SWITCH_PKG_${_lod_pkg}}")
    endif()
endforeach()
unset(_lod_pkg)
