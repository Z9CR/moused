cmake_minimum_required(VERSION 3.31)
project(moused CXX)
set(CMAKE_CXX_STANDARD 26)
# prog params
set(INCLUDES_DIR include)

# gather head files
set(INCLUDES "")
foreach(dir ${INCLUDES_DIR})
    file(GLOB dir_includes ${dir}/*.h ${dir}/*.hpp)
    list(APPEND INCLUDES ${dir_includes})
endforeach()

# platform-specific sources
set(COMMON_SOURCES src/main.cpp src/config.cpp src/ui.cpp src/utils.cpp src/macro.cpp src/macro_manager.cpp)
if(WIN32)
    set(PLATFORM_SOURCES src/win.cpp)
elseif(APPLE)
    set(PLATFORM_SOURCES src/mac.cpp)
elseif(LINUX)
    set(PLATFORM_SOURCES src/linux.cpp src/polkit_utils.cpp)
elseif(BSD)
    set(PLATFORM_SOURCES src/bsd.cpp src/polkit_utils.cpp)
endif()

include(FetchContent)

# ---- embed the tray icon PNG directly into the binary ----
# Turn assets/mousec.png into a C byte-array header so the resulting
# executable is a single self-contained file (no assets dir needed at
# runtime). This runs at configure time, which is fine for a static asset.
set(GENERATED_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${GENERATED_DIR})
file(READ ${CMAKE_CURRENT_SOURCE_DIR}/assets/mousec.png PNG_HEX HEX)
# "89504E470D0A1A0A..." -> "0x89, 0x50, 0x4E, ..."
string(REGEX REPLACE "(..)" "0x\\1, " PNG_BYTES "${PNG_HEX}")
string(REGEX REPLACE ", $" "" PNG_BYTES "${PNG_BYTES}")
set(PNG_HEADER ${GENERATED_DIR}/mousec_png.h)
file(WRITE ${PNG_HEADER}
"// Auto-generated from assets/mousec.png (do not edit).\n"
"#pragma once\n"
"#include <cstddef>\n"
"static const unsigned char mousec_png_data[] = {\n    ${PNG_BYTES}\n};\n"
"static const std::size_t mousec_png_size = sizeof(mousec_png_data);\n"
)

# toml11
FetchContent_Declare(
    toml11
    GIT_REPOSITORY https://github.com/ToruNiina/toml11.git
    GIT_TAG v4.4.0
)
FetchContent_MakeAvailable(toml11)

# wxWidget
FetchContent_Declare(
    wx
    GIT_REPOSITORY https://github.com/wxWidgets/wxWidgets
    GIT_TAG v3.3.3
)
# set compilation of `wxWidget` as `Static` force in case `DLL`(or SO) linking err
set(wxBUILD_SHARED OFF CACHE BOOL "" FORCE)

if(APPLE)
    # wxWidgets 3.3.3 + the current macOS SDK: building wx with C++20 or
    # newer turns the cross-enum arithmetic in ColorSyncDeprecated.h
    # (pulled in via Carbon/ApplicationServices) into a hard error.
    # wx itself fully supports C++17, so build only the wx subproject at
    # C++17 and keep the rest of moused on the project's C++26.
    set(_moused_saved_cxx_std "${CMAKE_CXX_STANDARD}")
    set(CMAKE_CXX_STANDARD 17)
    FetchContent_MakeAvailable(wx)
    set(CMAKE_CXX_STANDARD "${_moused_saved_cxx_std}")
    unset(_moused_saved_cxx_std)
else()
    FetchContent_MakeAvailable(wx)
endif()

if(WIN32)
    # when use macro `wxIMPLEMENT_APP(MyApp)`, the enter func is `WinMain`,
    # but, MSVC expects enter func `main`, and theres no `main` inside, so we need to
    # specify enter func
    add_executable(moused WIN32 ${COMMON_SOURCES} ${PLATFORM_SOURCES} ${INCLUDES})
elseif(APPLE)
    # same
    add_executable(moused MACOSX_BUNDLE ${COMMON_SOURCES} ${PLATFORM_SOURCES} ${INCLUDES})
else()
    add_executable(moused ${COMMON_SOURCES} ${PLATFORM_SOURCES} ${INCLUDES})
endif()

target_include_directories(moused PRIVATE ${INCLUDES_DIR} ${GENERATED_DIR})
target_link_libraries(moused PRIVATE toml11::toml11)
target_link_libraries(moused PRIVATE wx::core wx::base)

if(APPLE)
    # mac backend uses CoreGraphics (CGEvent*) and ApplicationServices
    # (AXIsProcessTrusted) to synthesize input & check Accessibility
    target_link_libraries(moused PRIVATE
        "-framework CoreGraphics"
        "-framework ApplicationServices")

    # Apple's libc++ hides std::jthread / std::stop_token (used by
    # main.cpp, macro.cpp, macro_manager.cpp) unless this macro is defined
    # on libc++ they are still gated as "incomplete experimental" features.
    target_compile_definitions(moused PRIVATE _LIBCPP_ENABLE_EXPERIMENTAL)
endif()

if(APPLE)
    # mac.cpp includes ApplicationServices/Quickdraw, whose
    # ColorSyncDeprecated.h does cross-enum arithmetic that is a hard error
    # in C++20+ (this Apple SDK header cannot be compiled with C++26).
    # mac.cpp itself needs no C++20 features, so build only it at C++17.
    set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/src/mac.cpp
        PROPERTIES COMPILE_OPTIONS "-std=gnu++17")
endif()

# i18n: compile every `i18n/<lang>.po` into `locale/<lang>/LC_MESSAGES/moused.mo`
# and deploy it next to the executable, where wxLocale::AddCatalog("moused")
# finds it at runtime. Requires GNU gettext's `msgfmt` on PATH; when it's not
# available the build still succeeds and `_()` simply shows the raw strings.
set(I18N_DIR ${CMAKE_CURRENT_SOURCE_DIR}/i18n)
find_program(MSGFMT_EXECUTABLE NAMES msgfmt msgfmt.exe)
if(MSGFMT_EXECUTABLE)
    # Where the executable actually lands: defaults to the build dir, unless
    # RUNTIME_OUTPUT_DIRECTORY redirects it or (on macOS) it is wrapped in an
    # .app bundle. Used below to decide whether "deploy next to the
    # executable" actually moves the .mo somewhere else than where msgfmt
    # wrote it (in the default layout they are the same directory, and the
    # copy would be a pointless self-copy).
    get_target_property(MOUSED_RUNTIME_DIR moused RUNTIME_OUTPUT_DIRECTORY)
    if(NOT MOUSED_RUNTIME_DIR)
        set(MOUSED_RUNTIME_DIR ${CMAKE_CURRENT_BINARY_DIR})
    endif()
    if(APPLE)
        string(APPEND MOUSED_RUNTIME_DIR "/${CMAKE_PROJECT_NAME}.app/Contents/MacOS")
    endif()

    file(GLOB I18N_PO_FILES ${I18N_DIR}/*.po)
    set(I18N_MO_FILES "")
    foreach(PO_FILE ${I18N_PO_FILES})
        get_filename_component(LANG ${PO_FILE} NAME_WE)
        set(MO_FILE ${CMAKE_CURRENT_BINARY_DIR}/locale/${LANG}/LC_MESSAGES/moused.mo)
        add_custom_command(
            OUTPUT ${MO_FILE}
            COMMAND ${CMAKE_COMMAND} -E make_directory
                    ${CMAKE_CURRENT_BINARY_DIR}/locale/${LANG}/LC_MESSAGES
            COMMAND ${MSGFMT_EXECUTABLE} -o ${MO_FILE} ${PO_FILE}
            DEPENDS ${PO_FILE}
            COMMENT "msgfmt: ${PO_FILE}"
        )
        list(APPEND I18N_MO_FILES ${MO_FILE})
        # Deploy next to the built executable, but only when that location
        # differs from where the catalog was generated (default layout: the
        # executable and build/locale/... both live in the build dir, so the
        # copy would be a self-copy).
        if(NOT "${MOUSED_RUNTIME_DIR}/locale/${LANG}/LC_MESSAGES" STREQUAL
                "${CMAKE_CURRENT_BINARY_DIR}/locale/${LANG}/LC_MESSAGES")
            add_custom_command(
                TARGET moused POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different ${MO_FILE}
                        $<TARGET_FILE_DIR:moused>/locale/${LANG}/LC_MESSAGES/moused.mo
            )
        endif()
    endforeach()
    add_custom_target(i18n_mo ALL DEPENDS ${I18N_MO_FILES})
    # Build the catalogs *before* moused links: the POST_BUILD deploy above
    # (and any consumer of the catalogs) requires the .mo files to already
    # exist. Without this, `make moused` or a parallel `make -j` could run
    # the deploy before msgfmt has produced moused.mo.
    add_dependencies(moused i18n_mo)
else()
    message(STATUS "msgfmt not found - i18n catalogs will not be built")
endif()
