cmake_minimum_required(VERSION 3.20)

project(lvgl_motion_demo C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(LV_CONF_PATH "${CMAKE_CURRENT_SOURCE_DIR}/lv_conf.h" CACHE STRING "LVGL configuration file" FORCE)
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "" FORCE)

include(FetchContent)

FetchContent_Declare(
    lvgl
    GIT_REPOSITORY https://github.com/lvgl/lvgl.git
    GIT_TAG v9.5.0
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(lvgl)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)

add_executable(lvgl_motion_demo
    src/main.c
    src/motion_demo.c
    src/generated/logos.c
)

target_include_directories(lvgl_motion_demo PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_compile_definitions(lvgl_motion_demo PRIVATE
    LV_CONF_INCLUDE_SIMPLE
)

target_link_libraries(lvgl_motion_demo PRIVATE
    lvgl
    PkgConfig::SDL2
)

if(WIN32)
    add_custom_command(TARGET lvgl_motion_demo POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "D:/msys64/mingw64/bin/SDL2.dll"
            "$<TARGET_FILE_DIR:lvgl_motion_demo>/SDL2.dll"
    )
endif()
