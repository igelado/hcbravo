# SPDX-License-Identifier: LGPL-2.1-only
add_library(hcbravo-conf STATIC conf.cpp)
set_target_properties(hcbravo-conf PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(hcbravo-conf PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(hcbravo-conf PUBLIC yaml-cpp::yaml-cpp)
hcbravo_harden(hcbravo-conf)

if(HCBRAVO_BUILD_PLUGIN OR HCBRAVO_BUILD_PROFILE_EDITOR)
    add_subdirectory(gui)
endif()
