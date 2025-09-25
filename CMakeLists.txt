cmake_minimum_required(VERSION 3.30)
project(HCBravo)

set(CMAKE_CXX_STANDARD 23)

include(cmake/CPM.cmake)
CPMAddPackage("gh:libusb/hidapi#hidapi-0.15.0")
CPMAddPackage("gh:jbeder/yaml-cpp#master")

set(hcbravo_SRC ${PROJECT_SOURCE_DIR}/src)
set(hcbravo_EXT ${PROJECT_SOURCE_DIR}/ext)

add_library(hcbravo SHARED)
target_sources(hcbravo PRIVATE
    ${hcbravo_SRC}/knob.cpp
    ${hcbravo_SRC}/led.cpp
    ${hcbravo_SRC}/main.cpp
    ${hcbravo_SRC}/profile.cpp
    ${hcbravo_SRC}/state.cpp
)
if(WIN32)
    target_compile_definitions(hcbravo PRIVATE IBM)
    target_compile_definitions(hcbravo PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_link_libraries(hcbravo PRIVATE ${hcbravo_EXT}/XPSDK411/SDK/Libraries/Win/XPLM_64.lib) 
elseif(UNIX)
    if(NOT APPLE)
        target_compile_definitions(hcbravo PRIVATE LIN)
        target_link_libraries(hcbravo PRIVATE ${hcbravo_EXT}/XPSDK411/SDK/Libraries/Lin/XPLM_64.so) 
    endif()
endif()
target_compile_definitions(hcbravo PRIVATE XPLM200 XPLM210 XPLM300 XPLM400)
target_include_directories(hcbravo PRIVATE ${hcbravo_EXT}/XPSDK411/SDK/CHeaders hidapi::include ${yaml-cpp_SOURCE_DIR}/include/yaml-cpp)
target_link_libraries(hcbravo PRIVATE hidapi::hidapi yaml-cpp::yaml-cpp)