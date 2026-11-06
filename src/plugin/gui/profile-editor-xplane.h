// SPDX-License-Identifier: LGPL-2.1-only
#ifndef HCBRAVO_PROFILE_EDITOR_XPLANE_H
#define HCBRAVO_PROFILE_EDITOR_XPLANE_H

#include <chrono>
#include <vector>

#include <XPLM/XPLMDisplay.h>
#include <XPLM/XPLMPanelGraphics.h>

#include "profile-editor.h"
#include "profile-editor-native.h"

struct ImGuiContext;
struct ImDrawData;

class xplane_profile_editor {
    profile_editor editor_;
    const std::filesystem::path & active_profile_path_;
    ImGuiContext * context_ = nullptr;
    XPLMWindowID window_ = nullptr;
    native_editor_api native_;
    decltype(&XPLMGetModifierKeys) modifier_keys_ = nullptr;
    void * font_texture_ = nullptr;
    unsigned int gl_font_texture_ = 0;
    std::chrono::steady_clock::time_point last_frame_;
    std::vector<XPLMDrawCall_t> draw_calls_;

    void prepare_font();
    void render_native(const ImDrawData & data, int left, int top);
    void render_opengl(const ImDrawData & data, int left, int top);

    static void draw(XPLMWindowID window, void * ref);
    static int mouse(XPLMWindowID window, int x, int y, XPLMMouseStatus status, void * ref);
    static void key(XPLMWindowID window, char character, XPLMKeyFlags flags,
                    char virtual_key, void * ref, int losing_focus);
    static int wheel(XPLMWindowID window, int x, int y, int axis, int clicks, void * ref);

public:
    xplane_profile_editor(const std::filesystem::path & directory,
                          const std::filesystem::path & active_profile_path);
    ~xplane_profile_editor();
    xplane_profile_editor(const xplane_profile_editor &) = delete;
    xplane_profile_editor & operator=(const xplane_profile_editor &) = delete;

    void show();
};

#endif
