# SPDX-License-Identifier: LGPL-2.1-only
add_library(hcbravo SHARED)
target_sources(hcbravo PRIVATE
    xplane-logger.cpp
    gui/profile-editor-xplane.cpp
    knob.cpp
    led.cpp
    main.cpp
    profile.cpp
    state.cpp
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
target_include_directories(hcbravo PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${XPlaneSDK_SOURCE_DIR}/CHeaders" hidapi::include ${yaml-cpp_SOURCE_DIR}/include/yaml-cpp)
target_link_libraries(hcbravo PRIVATE hidapi::hidapi hcbravo-editor OpenGL::GL)
target_compile_definitions(hcbravo PRIVATE GL_SILENCE_DEPRECATION)
set_target_properties(hcbravo PROPERTIES PREFIX "")
set_target_properties(hcbravo PROPERTIES SUFFIX ".xpl")
hcbravo_harden(hcbravo)

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
install(FILES "${PROJECT_SOURCE_DIR}/ext/Roboto-LICENSE.txt" DESTINATION ${hcbravo_plugin_DIR})
install(DIRECTORY "${PROJECT_SOURCE_DIR}/ext/AeroSoft/joystick configs" DESTINATION ${hcbravo_DIR})
install(DIRECTORY ${PROJECT_SOURCE_DIR}/profiles/ DESTINATION ${hcbravo_preferences_DIR} FILES_MATCHING PATTERN "*.prf")

set_target_properties(hcbravo PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}"
    LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")
