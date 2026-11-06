# SPDX-License-Identifier: LGPL-2.1-only
add_executable(hcbravo-profile-editor
    main.cpp
    directory-selector.cpp
    dataref-catalog.cpp
    ${PROJECT_SOURCE_DIR}/src/common/console-logger.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)

if(WIN32)
    set_target_properties(hcbravo-profile-editor PROPERTIES WIN32_EXECUTABLE TRUE)
    target_sources(hcbravo-profile-editor PRIVATE windows-main.cpp)
endif()
target_compile_definitions(hcbravo-profile-editor PRIVATE GL_SILENCE_DEPRECATION)
target_link_libraries(hcbravo-profile-editor PRIVATE hcbravo-editor glfw OpenGL::GL)
if(CURL_FOUND)
    target_compile_definitions(hcbravo-profile-editor PRIVATE HCBRAVO_ONLINE_DATAREFS)
    target_link_libraries(hcbravo-profile-editor PRIVATE CURL::libcurl)
endif()
set_target_properties(hcbravo-profile-editor PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")
