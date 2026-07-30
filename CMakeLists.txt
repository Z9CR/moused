cmake_minimum_required(VERSION 4.2)
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
set(COMMON_SOURCES src/main.cpp src/config.cpp src/ui.cpp)
if(WIN32)
    set(PLATFORM_SOURCES src/win.cpp)
elseif(APPLE)
    set(PLATFORM_SOURCES src/mac.cpp)
elseif(LINUX)
    set(PLATFORM_SOURCES src/linux.cpp src/polkit_utils.cpp)
elseif(BSD)
    set(PLATFORM_SOURCES src/bsd.cpp src/polkit_utils.cpp)
endif()

add_executable(moused ${COMMON_SOURCES} ${PLATFORM_SOURCES} ${INCLUDES})

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
    list(REMOVE_ITEM LUA_SOURCES ${lua_SOURCE_DIR}/lua.c ${lua_SOURCE_DIR}/ltests.c)
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


target_link_libraries(moused PRIVATE toml11::toml11)
target_link_libraries(moused PRIVATE lua)
target_link_libraries(moused PRIVATE LuaBridge)
target_include_directories(moused PRIVATE ${INCLUDES_DIR})