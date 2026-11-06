// SPDX-License-Identifier: LGPL-2.1-only
#include <gtest/gtest.h>
#include "profile-editor-native.h"
#include "profile-editor-clip.h"
#include "profile-editor-xplane.h"
#include <XPLM/XPLMGraphics.h>
#include <XPLM/XPLMDataAccess.h>
#include <XPLM/XPLMUtilities.h>

#include <cstring>

namespace {
// Addresses are only inspected for availability, never called in these tests.
int symbol;
void * all_symbols(const char *) { return &symbol; }
void * no_symbols(const char *) { return nullptr; }
void * missing_texture_cleanup(const char * name) {
    return std::strcmp(name, "XPLMDestroyTexture") == 0 ? nullptr : &symbol;
}
constexpr float identity[] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};

bool provide_native = false;
XPLMCreateWindow_t window_params {};
int created_textures = 0, destroyed_textures = 0, native_batches = 0, transform_depth = 0;
void * create_texture(const unsigned char *, int, int) { ++created_textures; return &symbol; }
void destroy_texture(void *) { ++destroyed_textures; }
void draw_calls(const XPLMMesh_t *, int, const XPLMDrawCall_t *) { ++native_batches; }
void push_transform() { ++transform_depth; }
void pop_transform() { --transform_depth; }
void transform(float, float) {}
}

// Mock the host, not the renderer. Native frames execute the real editor path
// without an OpenGL context; fallback window selection needs no GL resources.
extern "C" {
void * XPLMFindSymbol(const char * name) {
    if(!provide_native) return nullptr;
    if(std::strcmp(name, "XPLMCreateTexture") == 0) return reinterpret_cast<void *>(&create_texture);
    if(std::strcmp(name, "XPLMDestroyTexture") == 0) return reinterpret_cast<void *>(&destroy_texture);
    if(std::strcmp(name, "XPLMDrawCalls") == 0) return reinterpret_cast<void *>(&draw_calls);
    if(std::strcmp(name, "XPLMTransformPush") == 0) return reinterpret_cast<void *>(&push_transform);
    if(std::strcmp(name, "XPLMTransformPop") == 0) return reinterpret_cast<void *>(&pop_transform);
    if(std::strcmp(name, "XPLMTransformTranslate") == 0 || std::strcmp(name, "XPLMTransformScale") == 0)
        return reinterpret_cast<void *>(&transform);
    return nullptr;
}
XPLMDataRef XPLMFindDataRef(const char *) { return nullptr; }
int XPLMIsDataRefGood(XPLMDataRef) { return 0; }
void XPLMGetWindowGeometry(XPLMWindowID, int * left, int * top, int * right, int * bottom) {
    if(left) *left = 100;
    if(top) *top = 1000;
    if(right) *right = 1100;
    if(bottom) *bottom = 200;
}
void XPLMDestroyWindow(XPLMWindowID) {}
void XPLMGetScreenBoundsGlobal(int * left, int * top, int * right, int * bottom) {
    *left = 0; *top = 1080; *right = 1920; *bottom = 0;
}
XPLMWindowID XPLMCreateWindowEx(XPLMCreateWindow_t * params) {
    window_params = *params;
    return reinterpret_cast<XPLMWindowID>(&symbol);
}
void XPLMSetWindowTitle(XPLMWindowID, const char *) {}
void XPLMSetWindowResizingLimits(XPLMWindowID, int, int, int, int) {}
void XPLMSetWindowIsVisible(XPLMWindowID, int) {}
void XPLMBringWindowToFront(XPLMWindowID) {}
void XPLMGetMouseLocationGlobal(int * x, int * y) { *x = 0; *y = 0; }
int XPLMHasKeyboardFocus(XPLMWindowID) { return 0; }
void XPLMTakeKeyboardFocus(XPLMWindowID) {}
void XPLMGenerateTextureNumbers(int * textures, int) { *textures = 0; }
void XPLMSetGraphicsState(int, int, int, int, int, int, int) {}
void XPLMBindTexture2d(int, int) {}
}

TEST(EditorRenderer, OlderHostGetsLegacySizedOpenGLWindow) {
    provide_native = false;
    const std::filesystem::path active;
    xplane_profile_editor editor(std::filesystem::path(HCBRAVO_SOURCE_DIR) / "conf", active);
    editor.show();
    EXPECT_EQ(window_params.contentType, xplm_WindowContentTypeOpenGL);
    EXPECT_EQ(window_params.structSize, offsetof(XPLMCreateWindow_t, contentType));
}

TEST(EditorRenderer, NewerHostRendersNativeFramesAndReleasesItsTexture) {
    provide_native = true;
    created_textures = destroyed_textures = native_batches = transform_depth = 0;
    const std::filesystem::path active;
    {
        xplane_profile_editor editor(std::filesystem::path(HCBRAVO_SOURCE_DIR) / "conf", active);
        editor.show();
        EXPECT_EQ(window_params.contentType, xplm_WindowContentTypePanelGraphics);
        EXPECT_EQ(window_params.structSize, sizeof(XPLMCreateWindow_t));
        EXPECT_EQ(created_textures, 0);
        for(int i = 0; i < 2; ++i) window_params.drawWindowFunc(
            reinterpret_cast<XPLMWindowID>(&symbol), window_params.refcon);
        EXPECT_EQ(created_textures, 1);
        EXPECT_GT(native_batches, 0);
        EXPECT_EQ(transform_depth, 0);
    }
    EXPECT_EQ(destroyed_textures, 1);
}

TEST(EditorRenderer, NativeRequiresCompleteApi) {
    EXPECT_TRUE(static_cast<bool>(native_editor_api::load(all_symbols)));
    EXPECT_FALSE(static_cast<bool>(native_editor_api::load(no_symbols)));
    EXPECT_FALSE(static_cast<bool>(native_editor_api::load(missing_texture_cleanup)));
}

TEST(EditorRenderer, ClipAccountsForWindowPositionScaleAndViewportOrigin) {
    // Window origin (20, 80), Y down, in a 128x128 boxel screen.
    constexpr float model[] = {1,0,0,0, 0,-1,0,0, 0,0,1,0, 20,80,0,1};
    constexpr float projection[] = {.015625f,0,0,0, 0,.015625f,0,0, 0,0,1,0, -1,-1,0,1};
    const int viewport[] = {10, 30, 256, 256};
    EXPECT_EQ(editor_scissor({0,0,10,10}, model, projection, viewport, viewport),
              (std::array<int,4>{50,170,20,20}));
}

TEST(EditorRenderer, ClipIntersectsHostScissorAndRejectsOffscreenCommands) {
    const int viewport[] = {0, 0, 200, 200};
    const int outer[] = {110,110,20,20};
    EXPECT_EQ(editor_scissor({0,0,1,1}, identity, identity, viewport, outer),
              (std::array<int,4>{110,110,20,20}));
    const auto clipped = editor_scissor({-1,-1,-.5f,-.5f}, identity, identity, viewport, outer);
    EXPECT_EQ(clipped[2], 0);
    EXPECT_EQ(clipped[3], 0);
}
