cmake_minimum_required(VERSION 3.31)
project(moused CXX C)
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
set(COMMON_SOURCES src/main.cpp src/config.cpp src/ui.cpp src/utils.cpp src/script_binding.cpp src/macro.cpp src/macro_manager.cpp)
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
# toml11
FetchContent_Declare(
    toml11
    GIT_REPOSITORY https://github.com/ToruNiina/toml11.git
    GIT_TAG v4.4.0
)
FetchContent_MakeAvailable(toml11)

# lua
FetchContent_Declare(
    lua
    GIT_REPOSITORY "https://github.com/lua/lua.git"
    GIT_TAG "v5.4.4"
)
# lua has no CMakeLists.txt, so we must populate manually.
# FetchContent_Populate is deprecated in CMake >= 4.0 (CMP0169),
# but the replacement MakeAvailable requires a CMakeLists.txt.
cmake_policy(SET CMP0169 OLD)
FetchContent_GetProperties(lua)
if(NOT lua_POPULATED)
    FetchContent_Populate(lua)
    file(GLOB LUA_SOURCES ${lua_SOURCE_DIR}/*.c)
    list(REMOVE_ITEM LUA_SOURCES ${lua_SOURCE_DIR}/lua.c ${lua_SOURCE_DIR}/ltests.c ${lua_SOURCE_DIR}/onelua.c)
    add_library(lua STATIC ${LUA_SOURCES})
    target_include_directories(lua PUBLIC ${lua_SOURCE_DIR})
endif()

# luabridge (header-only, has its own CMakeLists.txt)
FetchContent_Declare(
    LuaBridge
    GIT_REPOSITORY "https://github.com/vinniefalco/LuaBridge.git"
    GIT_TAG "2.10"
)
FetchContent_MakeAvailable(LuaBridge)

# wxWidget
FetchContent_Declare(
    wx
    GIT_REPOSITORY https://github.com/wxWidgets/wxWidgets
    GIT_TAG v3.3.3
)
# set compilation of `wxWidget` as `Static` force in case `DLL`(or SO) linking err
set(wxBUILD_SHARED OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(wx)

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

target_include_directories(moused PRIVATE ${INCLUDES_DIR})
target_link_libraries(moused PRIVATE toml11::toml11)
target_link_libraries(moused PRIVATE lua)
target_link_libraries(moused PRIVATE LuaBridge)
target_link_libraries(moused PRIVATE wx::core wx::base)

# i18n: compile every `i18n/<lang>.po` into `locale/<lang>/LC_MESSAGES/moused.mo`
# and deploy it next to the executable, where wxLocale::AddCatalog("moused")
# finds it at runtime. Requires GNU gettext's `msgfmt` on PATH; when it's not
# available the build still succeeds and `_()` simply shows the raw strings.
set(I18N_DIR ${CMAKE_CURRENT_SOURCE_DIR}/i18n)
find_program(MSGFMT_EXECUTABLE NAMES msgfmt msgfmt.exe)
if(MSGFMT_EXECUTABLE)
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
        # deploy next to the built executable
        add_custom_command(
            TARGET moused POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${MO_FILE}
                    $<TARGET_FILE_DIR:moused>/locale/${LANG}/LC_MESSAGES/moused.mo
        )
    endforeach()
    add_custom_target(i18n_mo ALL DEPENDS ${I18N_MO_FILES})
else()
    message(STATUS "msgfmt not found - i18n catalogs will not be built")
endif()
