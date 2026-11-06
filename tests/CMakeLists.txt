# SPDX-License-Identifier: LGPL-2.1-only
#
# tests/CMakeLists.txt
# XPlane Plugin for HoneyComb Bravo Throttle Controller
#
# Copyright (C) 2005 Isaac Gelado


CPMAddPackage("gh:google/googletest@1.17.0")

include(GoogleTest)
include(CTest)

if(HCBRAVO_BUILD_PROFILE_EDITOR)
    add_executable(directory-paths-test profile-editor/directory-paths-test.cpp)
    target_include_directories(directory-paths-test PRIVATE ${PROJECT_SOURCE_DIR}/src/profile-editor)
    target_link_libraries(directory-paths-test PRIVATE GTest::gtest_main)
    gtest_discover_tests(directory-paths-test)
endif()

if(TARGET hcbravo-editor)
    add_executable(profile-editor-test common/gui/profile-editor-test.cpp
        ${PROJECT_SOURCE_DIR}/src/common/console-logger.cpp)
    target_link_libraries(profile-editor-test PRIVATE GTest::gtest_main hcbravo-editor)
    gtest_discover_tests(profile-editor-test)
endif()

if(HCBRAVO_BUILD_PLUGIN)
    add_executable(editor-renderer-test plugin/gui/editor-renderer-test.cpp
        ${PROJECT_SOURCE_DIR}/src/plugin/gui/profile-editor-xplane.cpp ${PROJECT_SOURCE_DIR}/src/common/console-logger.cpp)
    target_include_directories(editor-renderer-test PRIVATE ${PROJECT_SOURCE_DIR}/src/plugin ${PROJECT_SOURCE_DIR}/src/plugin/gui "${XPlaneSDK_SOURCE_DIR}/CHeaders")
    target_compile_definitions(editor-renderer-test PRIVATE XPLM=1 GL_SILENCE_DEPRECATION HCBRAVO_SOURCE_DIR="${PROJECT_SOURCE_DIR}" XPLM200 XPLM210 XPLM300 XPLM301 XPLM303 XPLM400 XPLM410 XPLM411 XPLM420 XPLM430 XPLM440)
    if(WIN32)
        target_compile_definitions(editor-renderer-test PRIVATE IBM)
    elseif(APPLE)
        target_compile_definitions(editor-renderer-test PRIVATE APL)
    else()
        target_compile_definitions(editor-renderer-test PRIVATE LIN)
    endif()
    target_link_libraries(editor-renderer-test PRIVATE GTest::gtest_main hcbravo-editor OpenGL::GL)
    gtest_discover_tests(editor-renderer-test)
endif()

set(hcbravo_TEST ${PROJECT_SOURCE_DIR}/tests)

add_executable(profile-test
    ${hcbravo_TEST}/plugin/profile-test.cpp
    ${PROJECT_SOURCE_DIR}/src/plugin/xplane-logger.cpp
    ${PROJECT_SOURCE_DIR}/src/plugin/profile.cpp
)

target_compile_definitions(profile-test PRIVATE HCBRAVO_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
target_include_directories(profile-test PRIVATE ${PROJECT_SOURCE_DIR}/src/plugin ${hcbravo_TEST}/XPSDK)
target_link_libraries(profile-test GTest::gtest_main hcbravo-conf)
gtest_discover_tests(profile-test)

add_executable(runtime-test
    plugin/runtime-test.cpp
    ${PROJECT_SOURCE_DIR}/src/plugin/main.cpp
    ${PROJECT_SOURCE_DIR}/src/plugin/knob.cpp
    ${PROJECT_SOURCE_DIR}/src/plugin/state.cpp
    ${PROJECT_SOURCE_DIR}/src/plugin/profile.cpp
    ${PROJECT_SOURCE_DIR}/src/plugin/led.cpp
    ${PROJECT_SOURCE_DIR}/src/plugin/xplane-logger.cpp
)
target_include_directories(runtime-test PRIVATE ${PROJECT_SOURCE_DIR}/src/plugin ${hcbravo_TEST}/XPSDK ${hcbravo_TEST}/HID)
target_compile_definitions(runtime-test PRIVATE HCBRAVO_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
target_link_libraries(runtime-test PRIVATE GTest::gtest_main hcbravo-conf)
gtest_discover_tests(runtime-test)

add_executable(conf-test
    ${hcbravo_TEST}/common/conf-test.cpp
    ${PROJECT_SOURCE_DIR}/src/plugin/xplane-logger.cpp
)

target_include_directories(conf-test PRIVATE ${hcbravo_TEST}/XPSDK)
target_compile_definitions(conf-test PRIVATE HCBRAVO_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
target_link_libraries(conf-test GTest::gtest_main hcbravo-conf)
gtest_discover_tests(conf-test)

# Track additions/removals during the next build as well as normal configuration.
file(GLOB configuration_files CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/conf/*.yaml")
if(NOT configuration_files)
    message(FATAL_ERROR "No configuration files found in conf")
endif()
set(configuration_parameters "")
set(configuration_names "")
foreach(configuration_file IN LISTS configuration_files)
    get_filename_component(configuration_name "${configuration_file}" NAME_WE)
    string(MAKE_C_IDENTIFIER "${configuration_name}" configuration_name)
    if(configuration_name IN_LIST configuration_names)
        message(FATAL_ERROR "Duplicate configuration test name: ${configuration_name}")
    endif()
    list(APPEND configuration_names "${configuration_name}")
    string(REPLACE "\\" "\\\\" configuration_path "${configuration_file}")
    string(REPLACE "\"" "\\\"" configuration_path "${configuration_path}")
    string(APPEND configuration_parameters "    {\"${configuration_name}\", \"${configuration_path}\"},\n")
    add_test(NAME "conf.${configuration_name}"
        COMMAND configuration-test "--gtest_filter=Bundled/ConfigurationTest.Valid/${configuration_name}")
    set_tests_properties("conf.${configuration_name}" PROPERTIES LABELS "configuration")
endforeach()
configure_file(common/configuration-files.h.in configuration-files.h @ONLY)

add_executable(configuration-test common/configuration-test.cpp ${PROJECT_SOURCE_DIR}/src/common/console-logger.cpp)
target_include_directories(configuration-test PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_link_libraries(configuration-test PRIVATE hcbravo-conf GTest::gtest_main)
gtest_discover_tests(configuration-test TEST_FILTER "ValidationTest.*")
