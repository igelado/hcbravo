# SPDX-License-Identifier: LGPL-2.1-only
#
# CMakeLists.txt
# XPlane Plugin for HoneyComb Bravo Throttle Controller
#
# Copyright (C) 2005 Isaac Gelado


cmake_minimum_required(VERSION 3.28)
option(HCBRAVO_BUILD_PLUGIN "Build the X-Plane plugin" ON)
option(HCBRAVO_BUILD_PROFILE_EDITOR "Build the standalone profile editor" OFF)
if(HCBRAVO_BUILD_PLUGIN AND CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
    message(NOTICE "Building Universal Plug-in...")
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
endif()

project(HCBravo)

set(CMAKE_CXX_STANDARD 23)

include(CTest)
include(cmake/CPM.cmake)
# Dependencies are pinned by full commit hash: tags are mutable, hashes are not.
CPMAddPackage(
    NAME yaml-cpp
    GITHUB_REPOSITORY jbeder/yaml-cpp
    GIT_TAG 56e3bb550c91fd7005566f19c079cb7a503223cf # yaml-cpp-0.9.0
    OPTIONS "YAML_MSVC_SHARED_RT ON"
)
set(hcbravo_SRC ${PROJECT_SOURCE_DIR}/src)

# Warnings and hardening for code shipped in the plugin: the plugin target and the
# configuration library it links (which parses untrusted YAML). Dependencies,
# tests and the validator keep their defaults.
function(hcbravo_harden target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /GS /guard:cf)
        get_target_property(target_type ${target} TYPE)
        if(NOT target_type STREQUAL "STATIC_LIBRARY")
            target_link_options(${target} PRIVATE /guard:cf /DYNAMICBASE)
        endif()
    else()
        target_compile_options(${target} PRIVATE
            -Wall -Wextra
            # Only the SDK's PLUGIN_API entry points are exported.
            -fvisibility=hidden -fvisibility-inlines-hidden
            $<$<CONFIG:Release>:-fstack-protector-strong>
            $<$<CONFIG:Release>:-U_FORTIFY_SOURCE>
            $<$<CONFIG:Release>:-D_FORTIFY_SOURCE=2>
        )
    endif()
endfunction()

add_library(hcbravo-conf STATIC ${hcbravo_SRC}/conf.cpp)
set_target_properties(hcbravo-conf PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(hcbravo-conf PUBLIC ${hcbravo_SRC})
target_link_libraries(hcbravo-conf PUBLIC yaml-cpp::yaml-cpp)
hcbravo_harden(hcbravo-conf)

add_executable(hcbravo-validate ${hcbravo_SRC}/conf-validator.cpp ${hcbravo_SRC}/console-logger.cpp)
target_link_libraries(hcbravo-validate PRIVATE hcbravo-conf)

if(HCBRAVO_BUILD_PLUGIN OR HCBRAVO_BUILD_PROFILE_EDITOR)
    CPMAddPackage(
        NAME imgui
        VERSION 1.91.9
        URL "https://codeload.github.com/ocornut/imgui/tar.gz/f5befd2d29e66809cd1110a152e375a7f1981f06" # v1.91.9b
        URL_HASH SHA256=85f4ce357df05bcc331b587f01976f47fb55f19fadf477a7907289686bc3f4c8
        DOWNLOAD_ONLY YES
    )
    add_library(hcbravo-imgui STATIC
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp
    )
    set_target_properties(hcbravo-imgui PROPERTIES POSITION_INDEPENDENT_CODE ON
        CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN ON)
    target_include_directories(hcbravo-imgui PUBLIC "${imgui_SOURCE_DIR}")

    # Embed the UI and annunciator fonts for both hosts.
    set(hcbravo_fonts "")
    foreach(font IN ITEMS "${PROJECT_SOURCE_DIR}/ext/Roboto-Regular.ttf"
                          "${imgui_SOURCE_DIR}/misc/fonts/Roboto-Medium.ttf")
        get_filename_component(font_name "${font}" NAME_WE)
        string(MAKE_C_IDENTIFIER "${font_name}" font_name)
        file(READ "${font}" font_bytes HEX)
        string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1,\n" font_bytes "${font_bytes}")
        string(APPEND hcbravo_fonts "static unsigned char ${font_name}[] = {\n${font_bytes}};\n")
    endforeach()
    file(CONFIGURE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/editor-fonts.h"
        CONTENT "@hcbravo_fonts@" @ONLY)

    add_library(hcbravo-editor STATIC ${hcbravo_SRC}/profile-editor.cpp)
    set_target_properties(hcbravo-editor PROPERTIES POSITION_INDEPENDENT_CODE ON)
    target_include_directories(hcbravo-editor PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
    target_link_libraries(hcbravo-editor PUBLIC hcbravo-conf hcbravo-imgui)
    hcbravo_harden(hcbravo-editor)
endif()

if(HCBRAVO_BUILD_PROFILE_EDITOR)
    CPMAddPackage(
        NAME glfw
        VERSION 3.4
        URL "https://codeload.github.com/glfw/glfw/tar.gz/7b6aead9fb88b3623e3b3725ebb42670cbe4c579"
        URL_HASH SHA256=169d49096e339caabbc12e1762273c5cb9130a535a282ba46cf5c14ab6e55694
        OPTIONS "GLFW_BUILD_EXAMPLES OFF" "GLFW_BUILD_TESTS OFF" "GLFW_BUILD_DOCS OFF" "GLFW_INSTALL OFF"
    )
    find_package(OpenGL REQUIRED)
    find_package(CURL QUIET)
    add_executable(hcbravo-profile-editor
        ${hcbravo_SRC}/profile-editor-main.cpp
        ${hcbravo_SRC}/dataref-catalog.cpp
        ${hcbravo_SRC}/console-logger.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
    )
    target_compile_definitions(hcbravo-profile-editor PRIVATE GL_SILENCE_DEPRECATION)
    target_link_libraries(hcbravo-profile-editor PRIVATE hcbravo-editor glfw OpenGL::GL)
    if(CURL_FOUND)
        target_compile_definitions(hcbravo-profile-editor PRIVATE HCBRAVO_ONLINE_DATAREFS)
        target_link_libraries(hcbravo-profile-editor PRIVATE CURL::libcurl)
    endif()
endif()

if(HCBRAVO_BUILD_PLUGIN)
    CPMAddPackage(
        NAME hidapi
        GITHUB_REPOSITORY libusb/hidapi
        GIT_TAG d6b2a974608dec3b76fb1e36c189f22b9cf3650c # hidapi-0.15.0
        OPTIONS "BUILD_SHARED_LIBS FALSE"
    )
    CPMAddPackage(
        NAME XPlaneSDK
        VERSION 4.4.0
        URL "https://github.com/X-Plane/xpsdk/releases/download/xpsdk_4.4.0-r1/XPSDK440-r1.zip"
        URL_HASH SHA256=73e6d8be3b0b3bf1fd703113e0b0df2aa854db753c66802d7379320dbc22f771
        DOWNLOAD_ONLY YES
    )

    add_library(hcbravo SHARED)
    target_sources(hcbravo PRIVATE
        ${hcbravo_SRC}/xplane-logger.cpp
        ${hcbravo_SRC}/profile-editor-xplane.cpp
        ${hcbravo_SRC}/knob.cpp
        ${hcbravo_SRC}/led.cpp
        ${hcbravo_SRC}/main.cpp
        ${hcbravo_SRC}/profile.cpp
        ${hcbravo_SRC}/state.cpp
    )

    if(WIN32)
        target_compile_definitions(hcbravo PRIVATE IBM)
        target_compile_definitions(hcbravo PRIVATE _CRT_SECURE_NO_WARNINGS)
        target_link_libraries(hcbravo PRIVATE "${XPlaneSDK_SOURCE_DIR}/Libraries/Win/XPLM_64.lib")
        set(hcbravo_os_DIR win_x64)
    elseif(UNIX)
        if(NOT APPLE)
            target_compile_definitions(hcbravo PRIVATE LIN)
            target_link_libraries(hcbravo PRIVATE "${XPlaneSDK_SOURCE_DIR}/Libraries/Lin/XPLM_64.so")
            set(hcbravo_os_DIR lin_x64)
        elseif(APPLE)
            target_compile_definitions(hcbravo PRIVATE APL)
            find_library(XPlaneSDK_XPLM_LIBRARY
                NAMES XPLM
                PATHS "${XPlaneSDK_SOURCE_DIR}/Libraries/Mac"
                NO_DEFAULT_PATH
                NO_CACHE
                REQUIRED
            )
            target_link_libraries(hcbravo PRIVATE "${XPlaneSDK_XPLM_LIBRARY}")
            set(hcbravo_os_DIR mac_x64)
        endif()
    endif()
    target_compile_definitions(hcbravo PRIVATE HCBRAVO_XPLANE_EDITOR XPLM200 XPLM210 XPLM300 XPLM301 XPLM303 XPLM400 XPLM410 XPLM411 XPLM420 XPLM430 XPLM440)
    target_include_directories(hcbravo PRIVATE "${XPlaneSDK_SOURCE_DIR}/CHeaders" hidapi::include ${yaml-cpp_SOURCE_DIR}/include/yaml-cpp)
    target_link_libraries(hcbravo PRIVATE hidapi::hidapi hcbravo-editor)
    set_target_properties(hcbravo PROPERTIES PREFIX "")
    set_target_properties(hcbravo PROPERTIES SUFFIX ".xpl")
    hcbravo_harden(hcbravo)

    set(CPACK_GENERATOR "ZIP")
    set(CPACK_PACKAGE_NAME "HCBravo")
    set(CPACK_PACKAGE_VERSION "0.0.1-alpha")
    set(CPACK_PACKAGE_FILE_NAME "hcbravo")
    set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY OFF)
    set(hcbravo_DIR "Resources")
    set(hcbravo_plugin_DIR "${hcbravo_DIR}/plugins/hcbravo")
    set(hcbravo_preferences_DIR "Output/preferences")
    install(TARGETS hcbravo
        RUNTIME DESTINATION ${hcbravo_plugin_DIR}/${hcbravo_os_DIR}
        LIBRARY DESTINATION ${hcbravo_plugin_DIR}/${hcbravo_os_DIR}
        COMPONENT BINARIES
    )
    install(DIRECTORY ${PROJECT_SOURCE_DIR}/conf DESTINATION ${hcbravo_plugin_DIR}
        PATTERN "copies" EXCLUDE)
    install(FILES "${imgui_SOURCE_DIR}/LICENSE.txt" DESTINATION ${hcbravo_plugin_DIR}
        RENAME Dear-ImGui-LICENSE.txt)
    install(FILES ext/Roboto-LICENSE.txt DESTINATION ${hcbravo_plugin_DIR})
    install(DIRECTORY "${PROJECT_SOURCE_DIR}/ext/AeroSoft/joystick configs" DESTINATION ${hcbravo_DIR})
    install(DIRECTORY ${PROJECT_SOURCE_DIR}/profiles/ DESTINATION ${hcbravo_preferences_DIR} FILES_MATCHING PATTERN "*.prf")
    include(CPack)
endif()

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
