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

if(HCBRAVO_BUILD_PLUGIN OR HCBRAVO_BUILD_PROFILE_EDITOR)
    find_package(OpenGL REQUIRED)
    CPMAddPackage(
        NAME imgui
        VERSION 1.91.9
        URL "https://codeload.github.com/ocornut/imgui/tar.gz/f5befd2d29e66809cd1110a152e375a7f1981f06" # v1.91.9b
        URL_HASH SHA256=85f4ce357df05bcc331b587f01976f47fb55f19fadf477a7907289686bc3f4c8
        DOWNLOAD_ONLY YES
    )
endif()

if(HCBRAVO_BUILD_PROFILE_EDITOR)
    CPMAddPackage(
        NAME glfw
        VERSION 3.4
        URL "https://codeload.github.com/glfw/glfw/tar.gz/7b6aead9fb88b3623e3b3725ebb42670cbe4c579"
        URL_HASH SHA256=169d49096e339caabbc12e1762273c5cb9130a535a282ba46cf5c14ab6e55694
        OPTIONS "GLFW_BUILD_EXAMPLES OFF" "GLFW_BUILD_TESTS OFF" "GLFW_BUILD_DOCS OFF" "GLFW_INSTALL OFF"
    )
    find_package(CURL QUIET)
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
endif()

add_subdirectory(src/common)
add_subdirectory(tools)
if(HCBRAVO_BUILD_PLUGIN)
    add_subdirectory(src/plugin)
    set(CPACK_GENERATOR "ZIP")
    set(CPACK_PACKAGE_NAME "HCBravo")
    set(CPACK_PACKAGE_VERSION "0.0.1-alpha")
    set(CPACK_PACKAGE_FILE_NAME "hcbravo")
    set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY OFF)
    include(CPack)
endif()
if(HCBRAVO_BUILD_PROFILE_EDITOR)
    add_subdirectory(src/profile-editor)
endif()
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
