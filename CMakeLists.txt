cmake_minimum_required(VERSION 3.20)

project(lvgl_motion_demo C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(LV_CONF_PATH "${CMAKE_CURRENT_SOURCE_DIR}/lv_conf.h" CACHE STRING "LVGL configuration file" FORCE)
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "" FORCE)

include(FetchContent)

find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(SDL2 IMPORTED_TARGET sdl2)
endif()

if(TARGET PkgConfig::SDL2)
    set(LVGL_MOTION_DEMO_SDL_TARGET PkgConfig::SDL2)
else()
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
    set(SDL_TESTS OFF CACHE BOOL "" FORCE)
    set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)

    FetchContent_Declare(
        SDL2
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG release-2.32.10
        GIT_SHALLOW TRUE
    )

    FetchContent_MakeAvailable(SDL2)
    set(LVGL_MOTION_DEMO_SDL_TARGET SDL2::SDL2-static)
endif()

FetchContent_Declare(
    lvgl
    GIT_REPOSITORY https://github.com/lvgl/lvgl.git
    GIT_TAG v9.5.0
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(lvgl)

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

if(APPLE)
    set_target_properties(lvgl_motion_demo PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "LVGL Motion Demo"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.local.lvgl-motion-demo"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0.0"
        MACOSX_BUNDLE_BUNDLE_VERSION "1"
        MACOSX_BUNDLE_COPYRIGHT "Local demo build"
    )
endif()

target_link_libraries(lvgl PRIVATE
    ${LVGL_MOTION_DEMO_SDL_TARGET}
)

target_link_libraries(lvgl_motion_demo PRIVATE
    lvgl
    ${LVGL_MOTION_DEMO_SDL_TARGET}
)

if(WIN32)
    add_custom_command(TARGET lvgl_motion_demo POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "D:/msys64/mingw64/bin/SDL2.dll"
            "$<TARGET_FILE_DIR:lvgl_motion_demo>/SDL2.dll"
    )
endif()
