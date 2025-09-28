cmake_minimum_required(VERSION 3.30)
if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
    message(NOTICE "Building Universal Plug-in...")
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
endif()

project(HCBravo)

set(CMAKE_CXX_STANDARD 23)

include(cmake/CPM.cmake)
CPMAddPackage(
    URI "gh:libusb/hidapi#hidapi-0.15.0"
    OPTIONS "BUILD_SHARED_LIBS FALSE"
)
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
    set(hcbravo_plugin_DIR win_x64)
elseif(UNIX)
    if(NOT APPLE)
        target_compile_definitions(hcbravo PRIVATE LIN)
        target_link_libraries(hcbravo PRIVATE ${hcbravo_EXT}/XPSDK411/SDK/Libraries/Lin/XPLM_64.so) 
        set(hcbravo_plugin_DIR lin_x64)
    elseif(APPLE)
        target_compile_definitions(hcbravo PRIVATE APL)
        find_library(XPLM_LIB
            NAME XPLM 
            PATHS ${hcbravo_EXT}/XPSDK411/SDK/Libraries/Mac
            REQUIRED
        )
        target_link_libraries(hcbravo PRIVATE ${XPLM_LIB})
        set(hcbravo_plugin_DIR mac_x64)
    endif()
endif()
target_compile_definitions(hcbravo PRIVATE XPLM200 XPLM210 XPLM300 XPLM400)
target_include_directories(hcbravo PRIVATE ${hcbravo_EXT}/XPSDK411/SDK/CHeaders hidapi::include ${yaml-cpp_SOURCE_DIR}/include/yaml-cpp)
target_link_libraries(hcbravo PRIVATE hidapi::hidapi yaml-cpp::yaml-cpp)
set_target_properties(hcbravo PROPERTIES PREFIX "")
set_target_properties(hcbravo PROPERTIES SUFFIX ".xpl")

enable_testing()
add_subdirectory(tests)


set(CPACK_GENERATOR "ZIP")
set(CPACK_PACKAGE_NAME "HCBravo")
set(CPACK_PACKAGE_VERSION "0.0.1-alpha")
set(CPACK_PACKAGE_FILE_NAME "hcbravo")
install(TARGETS hcbravo RUNTIME DESTINATION ${hcbravo_plugin_DIR} COMPONENT BINARIES)
install(DIRECTORY ${PROJECT_SOURCE_DIR}/conf DESTINATION .)
include(CPack)
