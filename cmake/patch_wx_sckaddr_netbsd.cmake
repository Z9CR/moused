# cmake/patch_wx_sckaddr_netbsd.cmake
#
# wxWidgets 3.3.3's src/common/sckaddr.cpp uses the glibc-style reentrant
# gethostbyname_r()/gethostbyaddr_r()/getservbyname_r() when wxWidgets'
# configure check (build/cmake/setup.cmake) reports them available.
#
# That check compiles its probe with the *C* compiler, where calling an
# undeclared function is only a warning, so on platforms that have no such
# functions at all -- e.g. NetBSD, whose <netdb.h> declares only the plain
# gethostbyname()/getservbyname() -- it can falsely succeed. sckaddr.cpp
# itself is compiled as C++, where an undeclared function is a hard error:
#
#   error: 'gethostbyname_r' was not declared in this scope
#
# This script forces the mutex-protected fallback (deep-copy + plain
# gethostbyname() & friends) on NetBSD by #undef-ing the HAVE_FUNC_*_R_*
# macros before sckaddr.cpp derives wxHAS_REENTRANT_* from them. Idempotent:
# re-running on an already-patched tree is a no-op.
#
# Usage (either way):
#   include(patch_wx_sckaddr_netbsd.cmake)  # after FetchContent_MakeAvailable(wx)
#   cmake -Dwx_src_dir=<wx source dir> -P patch_wx_sckaddr_netbsd.cmake

if(NOT DEFINED wx_src_dir)
    message(FATAL_ERROR "patch_wx_sckaddr_netbsd.cmake: wx_src_dir is not set")
endif()

set(_sckaddr "${wx_src_dir}/src/common/sckaddr.cpp")
if(NOT EXISTS "${_sckaddr}")
    message(FATAL_ERROR
        "patch_wx_sckaddr_netbsd.cmake: file not found: ${_sckaddr}")
endif()

file(READ "${_sckaddr}" _content)

if("${_content}" MATCHES "undef HAVE_FUNC_GETHOSTBYNAME_R_6")
    message(STATUS
        "patch_wx_sckaddr_netbsd.cmake: ${_sckaddr} already patched (no-op)")
else()
    string(REPLACE
"#ifdef HAVE_FUNC_GETHOSTBYNAME_R_5"
"#if defined(__NetBSD__)
    // NetBSD does not provide the glibc-style reentrant gethostbyname_r() &
    // friends, and wxWidgets' configure check can be fooled into thinking it
    // does: it compiles the probe in C mode, where a call to an undeclared
    // function is only a warning, while sckaddr.cpp itself is C++, where it
    // is a hard error. Force the mutex-protected fallback path on NetBSD.
    #undef HAVE_FUNC_GETHOSTBYNAME_R_5
    #undef HAVE_FUNC_GETHOSTBYNAME_R_6
    #undef HAVE_FUNC_GETHOSTBYADDR_R_5
    #undef HAVE_FUNC_GETHOSTBYADDR_R_6
    #undef HAVE_FUNC_GETSERVBYNAME_R_5
    #undef HAVE_FUNC_GETSERVBYNAME_R_6
#endif

#ifdef HAVE_FUNC_GETHOSTBYNAME_R_5"
        _content "${_content}")
    if(NOT "${_content}" MATCHES "undef HAVE_FUNC_GETHOSTBYNAME_R_6")
        message(FATAL_ERROR
            "patch_wx_sckaddr_netbsd.cmake: failed to patch ${_sckaddr}")
    endif()
    file(WRITE "${_sckaddr}" "${_content}")
    message(STATUS
        "patched ${_sckaddr}: disable *_r() netdb funcs on NetBSD")
endif()
