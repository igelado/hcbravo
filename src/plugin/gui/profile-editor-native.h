// SPDX-License-Identifier: LGPL-2.1-only
#ifndef HCBRAVO_PROFILE_EDITOR_NATIVE_H
#define HCBRAVO_PROFILE_EDITOR_NATIVE_H

#include <XPLM/XPLMPanelGraphics.h>

// These entry points must never become load-time imports: older hosts do not
// export them. A partially available API is treated as unavailable.
struct native_editor_api {
    decltype(&XPLMCreateTexture) create_texture = nullptr;
    decltype(&XPLMDestroyTexture) destroy_texture = nullptr;
    decltype(&XPLMDrawCalls) draw_calls = nullptr;
    decltype(&XPLMTransformPush) push = nullptr;
    decltype(&XPLMTransformPop) pop = nullptr;
    decltype(&XPLMTransformTranslate) translate = nullptr;
    decltype(&XPLMTransformScale) scale = nullptr;

    explicit operator bool() const noexcept {
        return create_texture && destroy_texture && draw_calls && push && pop && translate && scale;
    }

    static native_editor_api load(void * (*find)(const char *)) {
        native_editor_api api;
        api.create_texture = reinterpret_cast<decltype(api.create_texture)>(find("XPLMCreateTexture"));
        api.destroy_texture = reinterpret_cast<decltype(api.destroy_texture)>(find("XPLMDestroyTexture"));
        api.draw_calls = reinterpret_cast<decltype(api.draw_calls)>(find("XPLMDrawCalls"));
        api.push = reinterpret_cast<decltype(api.push)>(find("XPLMTransformPush"));
        api.pop = reinterpret_cast<decltype(api.pop)>(find("XPLMTransformPop"));
        api.translate = reinterpret_cast<decltype(api.translate)>(find("XPLMTransformTranslate"));
        api.scale = reinterpret_cast<decltype(api.scale)>(find("XPLMTransformScale"));
        return api;
    }
};

#endif
