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
    ${hcbravo_SRC}/profile.cpp
)

target_compile_definitions(profile-test PRIVATE ${xpsds_DEFINE})
target_include_directories(profile-test PRIVATE ${hcbravo_SRC} ${hcbravo_TEST}/XPSDK ${yaml-cpp_SOURCE_DIR}/include/yaml-cpp)
target_link_libraries(profile-test GTest::gtest_main yaml-cpp::yaml-cpp)
gtest_discover_tests(profile-test)