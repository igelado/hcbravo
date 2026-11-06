# SPDX-License-Identifier: LGPL-2.1-only
add_library(hcbravo-imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp
)
set_target_properties(hcbravo-imgui PROPERTIES POSITION_INDEPENDENT_CODE ON
    CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN ON)
target_include_directories(hcbravo-imgui PUBLIC "${imgui_SOURCE_DIR}")

# Embed the UI and annunciator fonts for both hosts.
set(hcbravo_fonts "")
foreach(font IN ITEMS "${PROJECT_SOURCE_DIR}/ext/Roboto-Regular.ttf"
                      "${imgui_SOURCE_DIR}/misc/fonts/Roboto-Medium.ttf")
    get_filename_component(font_name "${font}" NAME_WE)
    string(MAKE_C_IDENTIFIER "${font_name}" font_name)
    file(READ "${font}" font_bytes HEX)
    string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1,\n" font_bytes "${font_bytes}")
    string(APPEND hcbravo_fonts "static unsigned char ${font_name}[] = {\n${font_bytes}};\n")
endforeach()
file(CONFIGURE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/editor-fonts.h"
    CONTENT "@hcbravo_fonts@" @ONLY)

add_library(hcbravo-editor STATIC profile-editor.cpp)
set_target_properties(hcbravo-editor PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(hcbravo-editor PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}"
    PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_link_libraries(hcbravo-editor PUBLIC hcbravo-conf hcbravo-imgui)
hcbravo_harden(hcbravo-editor)
