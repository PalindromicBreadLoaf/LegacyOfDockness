include("${LOD_SWITCH_DEVKITPRO}/cmake/dkp-rule-overrides.cmake")

foreach (_lod_cxx_flag IN ITEMS -fexceptions -frtti)
    if (NOT CMAKE_CXX_FLAGS_INIT MATCHES "(^| )${_lod_cxx_flag}( |$)")
        string(APPEND CMAKE_CXX_FLAGS_INIT " ${_lod_cxx_flag}")
    endif()
endforeach()
unset(_lod_cxx_flag)
