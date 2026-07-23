cmake_minimum_required(VERSION 4.4)
project(moused CXX)
set(CMAKE_CXX_STANDARD 26)
# prog params
set(SOURCES_DIR src)
set(INCLUDES_DIR include)

# gather source files
set(SOURCES "")
foreach(dir ${SOURCES_DIR})
    file(GLOB dir_sources ${dir}/*.c ${dir}/*.cpp)
    list(APPEND SOURCES ${dir_sources})
endforeach()

# gather head files
set(INCLUDES "")
foreach(dir ${INCLUDES_DIR})
    file(GLOB dir_includes ${dir}/*.h ${dir}/*.hpp)
    list(APPEND INCLUDES ${dir_includes})
endforeach()

message(${SOURCES_DIR})
message(${INCLUDES_DIR})
add_executable(moused ${SOURCES} ${INCLUDES})
target_include_directories(moused PRIVATE ${INCLUDES_DIR})