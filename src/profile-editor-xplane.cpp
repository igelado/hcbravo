// SPDX-License-Identifier: LGPL-2.1-only
#include "profile-editor-xplane.h"

#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <type_traits>

#include <imgui.h>
#include <XPLM/XPLMDataAccess.h>

#include "logger.h"

namespace {

class scoped_context {
    ImGuiContext * previous_ = ImGui::GetCurrentContext();

public:
    explicit scoped_context(ImGuiContext * context) { ImGui::SetCurrentContext(context); }
    ~scoped_context() { ImGui::SetCurrentContext(previous_); }
};

void update_modifiers(ImGuiIO & io, XPLMKeyFlags flags) {
    io.AddKeyEvent(ImGuiMod_Shift, (flags & xplm_ShiftFlag) != 0);
    io.AddKeyEvent(ImGuiMod_Alt, (flags & xplm_OptionAltFlag) != 0);
#ifdef __APPLE__
    // XPLM reports Command and Control through the same flag on macOS.
    io.AddKeyEvent(ImGuiMod_Super, (flags & xplm_ControlFlag) != 0);
#else
    io.AddKeyEvent(ImGuiMod_Ctrl, (flags & xplm_ControlFlag) != 0);
#endif
}

ImGuiKey editor_key(unsigned char key) {
    if(key >= XPLM_VK_A && key <= XPLM_VK_Z) {
        return static_cast<ImGuiKey>(ImGuiKey_A + key - XPLM_VK_A);
    }
    switch(key) {
        case XPLM_VK_BACK: return ImGuiKey_Backspace;
        case XPLM_VK_DELETE: return ImGuiKey_Delete;
        case XPLM_VK_LEFT: return ImGuiKey_LeftArrow;
        case XPLM_VK_RIGHT: return ImGuiKey_RightArrow;
        case XPLM_VK_UP: return ImGuiKey_UpArrow;
        case XPLM_VK_DOWN: return ImGuiKey_DownArrow;
        case XPLM_VK_HOME: return ImGuiKey_Home;
        case XPLM_VK_END: return ImGuiKey_End;
        case XPLM_VK_RETURN: return ImGuiKey_Enter;
        case XPLM_VK_TAB: return ImGuiKey_Tab;
        case XPLM_VK_ESCAPE: return ImGuiKey_Escape;
        default: return ImGuiKey_None;
    }
}

void update_mouse(XPLMWindowID window, int x, int y) {
    int left, top;
    XPLMGetWindowGeometry(window, &left, &top, nullptr, nullptr);
    ImGui::GetIO().AddMousePosEvent(static_cast<float>(x - left), static_cast<float>(top - y));
}

} // namespace

xplane_profile_editor::xplane_profile_editor(const std::filesystem::path & directory,
                                             const std::filesystem::path & active_profile_path)
    : editor_(directory, [](const char * path) {
        const auto ref = XPLMFindDataRef(path);
        return ref != nullptr && XPLMIsDataRefGood(ref);
    }), active_profile_path_(active_profile_path) {
    scoped_context restore(ImGui::GetCurrentContext());
    IMGUI_CHECKVERSION();
    context_ = ImGui::CreateContext();
    ImGui::SetCurrentContext(context_);
    editor_.load_fonts();
    auto & io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;
    unsigned char * pixels;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    font_texture_ = XPLMCreateTexture(pixels, width, height);
    if(font_texture_ == nullptr) {
        ImGui::DestroyContext(context_);
        throw std::runtime_error("Could not create the editor font texture.");
    }
    io.Fonts->SetTexID(reinterpret_cast<ImTextureID>(font_texture_));
}

xplane_profile_editor::~xplane_profile_editor() {
    if(window_ != nullptr) XPLMDestroyWindow(window_);
    XPLMDestroyTexture(font_texture_);
    ImGui::DestroyContext(context_);
}

void xplane_profile_editor::show() {
    if(window_ == nullptr) {
        int left, top, right, bottom;
        XPLMGetScreenBoundsGlobal(&left, &top, &right, &bottom);
        const int width = std::min(profile_editor::initial_width, right - left);
        const int height = std::min(profile_editor::initial_height, top - bottom);
        left += (right - left - width) / 2;
        top -= (top - bottom - height) / 2;

        XPLMCreateWindow_t params {};
        params.structSize = sizeof(params);
        params.left = left;
        params.top = top;
        params.right = left + width;
        params.bottom = top - height;
        params.visible = 1;
        params.drawWindowFunc = &draw;
        params.handleMouseClickFunc = &mouse;
        params.handleKeyFunc = &key;
        params.handleMouseWheelFunc = &wheel;
        params.refcon = this;
        params.decorateAsFloatingWindow = xplm_WindowDecorationRoundRectangle;
        params.layer = xplm_WindowLayerFloatingWindows;
        params.contentType = xplm_WindowContentTypePanelGraphics;

        window_ = XPLMCreateWindowEx(&params);
        if(window_ == nullptr) throw std::runtime_error("Could not create the profile editor window.");
        XPLMSetWindowTitle(window_, "HCBravo Aircraft Profile Editor");
        XPLMSetWindowResizingLimits(window_, 320, 220, 1200, 1000);
    }
    XPLMSetWindowIsVisible(window_, 1);
    XPLMBringWindowToFront(window_);
}

void xplane_profile_editor::draw(XPLMWindowID window, void * ref) {
    auto & self = *static_cast<xplane_profile_editor *>(ref);
    scoped_context context(self.context_);
    auto & io = ImGui::GetIO();
    int left, top, right, bottom;
    XPLMGetWindowGeometry(window, &left, &top, &right, &bottom);
    if(right <= left || top <= bottom) return;
    io.DisplaySize = ImVec2(static_cast<float>(right - left), static_cast<float>(top - bottom));

    const auto now = std::chrono::steady_clock::now();
    io.DeltaTime = std::clamp(std::chrono::duration<float>(now - self.last_frame_).count(),
                              0.001f, 0.1f);
    self.last_frame_ = now;
    update_modifiers(io, XPLMGetModifierKeys());
    int mouse_x, mouse_y;
    XPLMGetMouseLocationGlobal(&mouse_x, &mouse_y);
    update_mouse(window, mouse_x, mouse_y);

    ImGui::NewFrame();
    const bool close = self.editor_.draw(self.active_profile_path_);
    ImGui::Render();
    if(close) {
        if(XPLMHasKeyboardFocus(window)) XPLMTakeKeyboardFocus(nullptr);
        XPLMSetWindowIsVisible(window, 0);
        return;
    }

    static_assert(std::is_same_v<ImDrawIdx, uint16_t>);
    static_assert(sizeof(ImDrawVert) == 5 * sizeof(float) &&
                  offsetof(ImDrawVert, uv) == 2 * sizeof(float) &&
                  offsetof(ImDrawVert, col) == 4 * sizeof(float));
    try {
        for(const auto * list : ImGui::GetDrawData()->CmdLists) {
            self.draw_calls_.clear();
            self.draw_calls_.reserve(list->CmdBuffer.Size);
            for(const auto & cmd : list->CmdBuffer) {
                self.draw_calls_.push_back({
                    reinterpret_cast<void *>(cmd.GetTexID()),
                    {cmd.ClipRect.x, cmd.ClipRect.y, cmd.ClipRect.z, cmd.ClipRect.w},
                    static_cast<int>(cmd.IdxOffset), static_cast<int>(cmd.ElemCount),
                    static_cast<int>(cmd.VtxOffset)
                });
            }
            const XPLMMesh_t mesh {
                list->VtxBuffer.Size, reinterpret_cast<const float *>(list->VtxBuffer.Data),
                list->IdxBuffer.Size, list->IdxBuffer.Data
            };
            // SDK 4.4.0-r1 consumes panel coordinates under the current transform.
            XPLMTransformPush();
            XPLMTransformTranslate(static_cast<float>(left), static_cast<float>(top));
            XPLMTransformScale(1.0f, -1.0f);
            XPLMDrawCalls(&mesh, static_cast<int>(self.draw_calls_.size()), self.draw_calls_.data());
            XPLMTransformPop();
        }
    }
    catch(const std::exception & error) {
        logger::info() << "Could not render the profile editor: " << error.what();
    }
}

int xplane_profile_editor::mouse(XPLMWindowID window, int x, int y, XPLMMouseStatus status, void * ref) {
    auto & self = *static_cast<xplane_profile_editor *>(ref);
    scoped_context context(self.context_);
    update_mouse(window, x, y);
    if(status == xplm_MouseDown) {
        XPLMBringWindowToFront(window);
        XPLMTakeKeyboardFocus(window);
    }
    ImGui::GetIO().AddMouseButtonEvent(0, status != xplm_MouseUp);
    return 1;
}

void xplane_profile_editor::key(XPLMWindowID, char character, XPLMKeyFlags flags,
                                char virtual_key, void * ref, int losing_focus) {
    auto & self = *static_cast<xplane_profile_editor *>(ref);
    scoped_context context(self.context_);
    auto & io = ImGui::GetIO();
    if(losing_focus) {
        io.AddFocusEvent(false);
        return;
    }
    io.AddFocusEvent(true);
    update_modifiers(io, flags);
    const auto mapped = editor_key(static_cast<unsigned char>(virtual_key));
    const bool down = (flags & xplm_UpFlag) == 0;
    if(mapped != ImGuiKey_None) io.AddKeyEvent(mapped, down);
    if(down && !(flags & xplm_ControlFlag) && character >= ' ' && character <= '~') {
        io.AddInputCharacter(static_cast<unsigned char>(character));
    }
}

int xplane_profile_editor::wheel(XPLMWindowID window, int x, int y, int axis, int clicks, void * ref) {
    auto & self = *static_cast<xplane_profile_editor *>(ref);
    scoped_context context(self.context_);
    update_mouse(window, x, y);
    ImGui::GetIO().AddMouseWheelEvent(axis == 1 ? static_cast<float>(clicks) : 0.0f,
                                      axis == 0 ? static_cast<float>(clicks) : 0.0f);
    return 1;
}
