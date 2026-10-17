# SPDX-License-Identifier: LGPL-2.1-only
#
# tests/CMakeLists.txt
# XPlane Plugin for HoneyComb Bravo Throttle Controller
#
# Copyright (C) 2005 Isaac Gelado


CPMAddPackage("gh:google/googletest@1.17.0")

include(GoogleTest)
include(CTest)

set(hcbravo_TEST ${PROJECT_SOURCE_DIR}/tests)

add_executable(profile-test
    ${hcbravo_TEST}/profile-test.cpp
    ${hcbravo_SRC}/xplane-logger.cpp
    ${hcbravo_SRC}/profile.cpp
)

target_compile_definitions(profile-test PRIVATE ${xpsds_DEFINE})
target_compile_definitions(profile-test PRIVATE HCBRAVO_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
target_include_directories(profile-test PRIVATE ${hcbravo_SRC} ${hcbravo_TEST}/XPSDK ${yaml-cpp_SOURCE_DIR}/include/yaml-cpp)
target_link_libraries(profile-test GTest::gtest_main hcbravo-conf)
gtest_discover_tests(profile-test)

add_executable(conf-test
    ${hcbravo_TEST}/conf-test.cpp
    ${hcbravo_SRC}/xplane-logger.cpp
)

target_include_directories(conf-test PRIVATE ${hcbravo_SRC} ${hcbravo_TEST}/XPSDK ${yaml-cpp_SOURCE_DIR}/include/yaml-cpp)
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
configure_file(configuration-files.h.in configuration-files.h @ONLY)

add_executable(configuration-test configuration-test.cpp ${hcbravo_SRC}/console-logger.cpp)
target_include_directories(configuration-test PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_link_libraries(configuration-test PRIVATE hcbravo-conf GTest::gtest_main)
gtest_discover_tests(configuration-test TEST_FILTER "ValidationTest.*")
