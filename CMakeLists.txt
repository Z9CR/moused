cmake_minimum_required(VERSION 4.4)
project(moused CXX)
set(CMAKE_CXX_STANDARD 26)
# prog params
set(SOURCES_DIR src)
set(INCLUDES_DIR include)

# gather head files
set(INCLUDES "")
foreach(dir ${INCLUDES_DIR})
    file(GLOB dir_includes ${dir}/*.h ${dir}/*.hpp)
    list(APPEND INCLUDES ${dir_includes})
endforeach()

# platform-specific sources
set(COMMON_SOURCES src/main.cpp src/config.cpp)
if(WIN32)
    set(PLATFORM_SOURCES src/win.cpp)
elseif(APPLE)
    set(PLATFORM_SOURCES src/mac.cpp)
elseif(LINUX)
    set(PLATFORM_SOURCES src/linux.cpp)
elseif(BSD)
    set(PLATFORM_SOURCES src/bsd.cpp)
endif()

add_executable(moused ${COMMON_SOURCES} ${PLATFORM_SOURCES} ${INCLUDES})
target_include_directories(moused PRIVATE ${INCLUDES_DIR})
