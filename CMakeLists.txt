# SPDX-License-Identifier: LGPL-2.1-only
#
# CMakeLists.txt
# XPlane Plugin for HoneyComb Bravo Throttle Controller
#
# Copyright (C) 2005 Isaac Gelado


cmake_minimum_required(VERSION 3.28)
if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
    message(NOTICE "Building Universal Plug-in...")
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
endif()

project(HCBravo)

set(CMAKE_CXX_STANDARD 23)

include(cmake/CPM.cmake)
# Dependencies are pinned by full commit hash: tags are mutable, hashes are not.
CPMAddPackage(
    NAME hidapi
    GITHUB_REPOSITORY libusb/hidapi
    GIT_TAG d6b2a974608dec3b76fb1e36c189f22b9cf3650c # hidapi-0.15.0
    OPTIONS "BUILD_SHARED_LIBS FALSE"
)
CPMAddPackage(
    NAME yaml-cpp
    GITHUB_REPOSITORY jbeder/yaml-cpp
    GIT_TAG 56e3bb550c91fd7005566f19c079cb7a503223cf # yaml-cpp-0.9.0
    OPTIONS "YAML_MSVC_SHARED_RT ON"
)
CPMAddPackage(
    NAME XPlaneSDK
    VERSION 4.3.0
    URL "https://developer.x-plane.com/wp-content/plugins/code-sample-generation/sdk_zip_files/XPSDK430.zip"
    URL_HASH SHA256=b9875ab27b593927b4f9b3e0ddfffe7401ee5dce6d86b50aea0da65f70ff7816
    DOWNLOAD_ONLY YES
)

set(hcbravo_SRC ${PROJECT_SOURCE_DIR}/src)

add_library(hcbravo-conf STATIC ${hcbravo_SRC}/conf.cpp)
set_target_properties(hcbravo-conf PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(hcbravo-conf PUBLIC ${hcbravo_SRC})
target_link_libraries(hcbravo-conf PUBLIC yaml-cpp::yaml-cpp)

add_executable(hcbravo-validate ${hcbravo_SRC}/conf-validator.cpp ${hcbravo_SRC}/console-logger.cpp)
target_link_libraries(hcbravo-validate PRIVATE hcbravo-conf)

add_library(hcbravo SHARED)
target_sources(hcbravo PRIVATE
    ${hcbravo_SRC}/xplane-logger.cpp
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
target_compile_definitions(hcbravo PRIVATE XPLM200 XPLM210 XPLM300 XPLM400 XPLM410)
target_include_directories(hcbravo PRIVATE "${XPlaneSDK_SOURCE_DIR}/CHeaders" hidapi::include ${yaml-cpp_SOURCE_DIR}/include/yaml-cpp)
target_link_libraries(hcbravo PRIVATE hidapi::hidapi yaml-cpp::yaml-cpp hcbravo-conf)
set_target_properties(hcbravo PROPERTIES PREFIX "")
set_target_properties(hcbravo PROPERTIES SUFFIX ".xpl")

# Warnings and hardening for the plugin itself (dependencies and tests keep their defaults).
if(MSVC)
    target_compile_options(hcbravo PRIVATE /W4 /GS /guard:cf)
    target_link_options(hcbravo PRIVATE /guard:cf /DYNAMICBASE)
else()
    target_compile_options(hcbravo PRIVATE
        -Wall -Wextra
        # Only the SDK's PLUGIN_API entry points are exported.
        -fvisibility=hidden -fvisibility-inlines-hidden
        $<$<CONFIG:Release>:-fstack-protector-strong>
        $<$<CONFIG:Release>:-U_FORTIFY_SOURCE>
        $<$<CONFIG:Release>:-D_FORTIFY_SOURCE=2>
    )
endif()

enable_testing()
# Keep tests registered from the project root for CI path filters.
add_subdirectory(tests)


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
install(DIRECTORY ${PROJECT_SOURCE_DIR}/conf DESTINATION ${hcbravo_plugin_DIR})
install(DIRECTORY "${PROJECT_SOURCE_DIR}/ext/AeroSoft/joystick configs" DESTINATION ${hcbravo_DIR})
install(DIRECTORY ${PROJECT_SOURCE_DIR}/profiles/ DESTINATION ${hcbravo_preferences_DIR} FILES_MATCHING PATTERN "*.prf")
include(CPack)
