cmake_minimum_required(VERSION 4.2)
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

# slint 
if (NOT Slint_FOUND)
  message("Slint could not be located in the CMake module search path. Downloading it from Git and building it locally")
  include(FetchContent)
  FetchContent_Declare(
    Slint
    GIT_REPOSITORY https://github.com/slint-ui/slint.git
    # `release/1` will auto-upgrade to the latest Slint >= 1.0.0 and < 2.0.0
    # `release/1.0` will auto-upgrade to the latest Slint >= 1.0.0 and < 1.1.0
    GIT_TAG release/1
    SOURCE_SUBDIR api/cpp
  )
  FetchContent_MakeAvailable(Slint)
  # Slint's CMakeLists.txt omits system-tray from its feature list,
  # but the Cargo feature exists and is required for SystemTrayIcon.
  set_property(TARGET slint_cpp APPEND PROPERTY CORROSION_FEATURES system-tray)
endif (NOT Slint_FOUND)

# toml11
include(FetchContent)
FetchContent_Declare(
  toml11
  GIT_REPOSITORY https://github.com/ToruNiina/toml11.git
  GIT_TAG        v4.4.0
)
FetchContent_MakeAvailable(toml11)

target_link_libraries(moused PRIVATE Slint::Slint)
target_link_libraries(moused PRIVATE toml11::toml11)
target_include_directories(moused PRIVATE ${INCLUDES_DIR})

# Slint's Rust backend (glutin/winit) needs WGL + IMM32 on Windows
if(WIN32)
    target_link_libraries(moused PRIVATE opengl32 imm32)
endif()

slint_target_sources(moused ui/mainwindow.slint)
