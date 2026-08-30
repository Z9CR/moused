# cmake/patch_wx_nanosvg_isnan.cmake
#
# wxWidgets 3.3.3 bundles a nanosvg fork whose nsvg__clampf() calls the
# unqualified isNaN(a). When wxWidgets compiles src/generic/bmpsvg.cpp as C++
# (it pulls in 3rdparty/nanosvg/src/nanosvgrast.h), NetBSD's <math.h> does not
# put isNaN into the global namespace -- only std::isNaN from <cmath> exists --
# so the build dies at nanosvgrast.h:961:
#
#   error: 'isnan' was not declared in this scope; did you mean 'std::isnan'?
#
# glibc/MSVC expose isNaN globally, which is why this only bites on NetBSD.
#
# This script rewrites that check to `a != a`, which is true exactly for NaN,
# is valid in both C and C++, and needs no <math.h> symbol at all. It is
# idempotent: re-running it on an already-patched tree is a no-op.
#
# Usage (either way):
#   include(patch_wx_nanosvg_isnan.cmake)   # after FetchContent_MakeAvailable(wx)
#   cmake -Dwx_src_dir=<wx source dir> -P patch_wx_nanosvg_isnan.cmake

if(NOT DEFINED wx_src_dir)
    message(FATAL_ERROR "patch_wx_nanosvg_isnan.cmake: wx_src_dir is not set")
endif()

set(_nsvg_file "${wx_src_dir}/3rdparty/nanosvg/src/nanosvgrast.h")
if(NOT EXISTS "${_nsvg_file}")
    message(FATAL_ERROR "patch_wx_nanosvg_isnan.cmake: file not found: ${_nsvg_file}")
endif()

file(READ "${_nsvg_file}" _nsvg_content)

if("${_nsvg_content}" MATCHES "isnan\\(a\\)")
    string(REPLACE "if (isnan(a))" "if (a != a) /* NaN: only NaN compares unequal to itself */"
        _nsvg_content "${_nsvg_content}")
    if("${_nsvg_content}" MATCHES "isnan\\(a\\)")
        message(FATAL_ERROR
            "patch_wx_nanosvg_isnan.cmake: failed to replace isnan(a) in ${_nsvg_file}")
    endif()
    file(WRITE "${_nsvg_file}" "${_nsvg_content}")
    message(STATUS "patched ${_nsvg_file}: isnan(a) -> a != a (NetBSD build fix)")
else()
    message(STATUS "patch_wx_nanosvg_isnan.cmake: ${_nsvg_file} has no isnan(a) to replace (no-op)")
endif()
