// SPDX-License-Identifier: LGPL-2.1-only
#include "profile-editor-xplane.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#ifdef __APPLE__
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif
#include <stdexcept>
#include <type_traits>

#include <imgui.h>
#include <XPLM/XPLMDataAccess.h>
#include <XPLM/XPLMGraphics.h>
#include <XPLM/XPLMUtilities.h>

#include "profile-editor-clip.h"

#include "logger.h"

namespace {

class scoped_context {
    ImGuiContext * previous_ = ImGui::GetCurrentContext();

public:
    explicit scoped_context(ImGuiContext * context) { ImGui::SetCurrentContext(context); }
    ~scoped_context() { ImGui::SetCurrentContext(previous_); }
};

class scoped_gl_state {
public:
    scoped_gl_state() {
        glPushAttrib(GL_ALL_ATTRIB_BITS);
        glPushClientAttrib(GL_CLIENT_VERTEX_ARRAY_BIT);
        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
    }
    ~scoped_gl_state() {
        glMatrixMode(GL_MODELVIEW);
        glPopMatrix();
        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
        glPopClientAttrib();
        glPopAttrib();
    }
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
    native_ = native_editor_api::load(&XPLMFindSymbol);
    modifier_keys_ = reinterpret_cast<decltype(modifier_keys_)>(XPLMFindSymbol("XPLMGetModifierKeys"));
    logger::info() << "Profile editor renderer: " << (native_ ? "native panel graphics" : "OpenGL fallback");
}

xplane_profile_editor::~xplane_profile_editor() {
    if(window_ != nullptr) XPLMDestroyWindow(window_);
    if(font_texture_) native_.destroy_texture(font_texture_);
    if(gl_font_texture_) glDeleteTextures(1, &gl_font_texture_);
    ImGui::DestroyContext(context_);
}

void xplane_profile_editor::prepare_font() {
    if(font_texture_ || gl_font_texture_) return;
    auto & io = ImGui::GetIO();
    unsigned char * pixels;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    if(native_) {
        font_texture_ = native_.create_texture(pixels, width, height);
        if(!font_texture_) throw std::runtime_error("Could not create the editor font texture.");
        io.Fonts->SetTexID(reinterpret_cast<ImTextureID>(font_texture_));
    }
    else {
        // GPU resources are created lazily from a window draw callback, where
        // X-Plane has made its OpenGL bridge context current.
        XPLMSetGraphicsState(0, 1, 0, 0, 1, 0, 0);
        XPLMBindTexture2d(0, 0);
        glPushAttrib(GL_TEXTURE_BIT);
        glPushClientAttrib(GL_CLIENT_PIXEL_STORE_BIT);
        int texture = 0;
        XPLMGenerateTextureNumbers(&texture, 1);
        gl_font_texture_ = static_cast<GLuint>(texture);
        glBindTexture(GL_TEXTURE_2D, gl_font_texture_);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        // Windows' system GL header only exposes 1.1 constants.
        constexpr GLint clamp_to_edge = 0x812F;
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, clamp_to_edge);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, clamp_to_edge);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
        glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
        glPopClientAttrib();
        glPopAttrib();
        if(!gl_font_texture_) {
            if(gl_font_texture_) glDeleteTextures(1, &gl_font_texture_);
            gl_font_texture_ = 0;
            throw std::runtime_error("Could not create the OpenGL editor font texture.");
        }
        io.Fonts->SetTexID(static_cast<ImTextureID>(gl_font_texture_));
    }
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
        // Older hosts only understand the pre-4.4 portion of this struct.
        params.structSize = native_ ? sizeof(params) : offsetof(XPLMCreateWindow_t, contentType);
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
        params.contentType = native_ ? xplm_WindowContentTypePanelGraphics : xplm_WindowContentTypeOpenGL;

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
    if(self.modifier_keys_) update_modifiers(io, self.modifier_keys_());
    int mouse_x, mouse_y;
    XPLMGetMouseLocationGlobal(&mouse_x, &mouse_y);
    update_mouse(window, mouse_x, mouse_y);

    try {
        self.prepare_font();
    }
    catch(const std::exception & error) {
        logger::info() << error.what();
        XPLMSetWindowIsVisible(window, 0);
        return;
    }
    ImGui::NewFrame();
    const bool close = self.editor_.draw(self.active_profile_path_);
    ImGui::Render();
    if(close) {
        if(XPLMHasKeyboardFocus(window)) XPLMTakeKeyboardFocus(nullptr);
        XPLMSetWindowIsVisible(window, 0);
        return;
    }

    try {
        if(self.native_) self.render_native(*ImGui::GetDrawData(), left, top);
        else self.render_opengl(*ImGui::GetDrawData(), left, top);
    }
    catch(const std::exception & error) {
        logger::info() << "Could not render the profile editor: " << error.what();
    }
}

void xplane_profile_editor::render_native(const ImDrawData & data, int left, int top) {
    static_assert(std::is_same_v<ImDrawIdx, uint16_t>);
    static_assert(sizeof(ImDrawVert) == 5 * sizeof(float) &&
                  offsetof(ImDrawVert, uv) == 2 * sizeof(float) &&
                  offsetof(ImDrawVert, col) == 4 * sizeof(float));
    for(const auto * list : data.CmdLists) {
        draw_calls_.clear();
        draw_calls_.reserve(list->CmdBuffer.Size);
        const XPLMMesh_t mesh {
            list->VtxBuffer.Size, reinterpret_cast<const float *>(list->VtxBuffer.Data),
            list->IdxBuffer.Size, list->IdxBuffer.Data
        };
        const auto flush = [&] {
            if(draw_calls_.empty()) return;
            native_.push();
            native_.translate(static_cast<float>(left), static_cast<float>(top));
            native_.scale(1.0f, -1.0f);
            native_.draw_calls(&mesh, static_cast<int>(draw_calls_.size()), draw_calls_.data());
            native_.pop();
            draw_calls_.clear();
        };
        for(const auto & cmd : list->CmdBuffer) {
            if(cmd.UserCallback) {
                flush();
                if(cmd.UserCallback != ImDrawCallback_ResetRenderState) cmd.UserCallback(list, &cmd);
                continue;
            }
            draw_calls_.push_back({
                reinterpret_cast<void *>(cmd.GetTexID()),
                {cmd.ClipRect.x, cmd.ClipRect.y, cmd.ClipRect.z, cmd.ClipRect.w},
                static_cast<int>(cmd.IdxOffset), static_cast<int>(cmd.ElemCount),
                static_cast<int>(cmd.VtxOffset)
            });
        }
        flush();
    }
}

void xplane_profile_editor::render_opengl(const ImDrawData & data, int left, int top) {
    // Set managed state through the SDK before saving GL state so X-Plane's
    // state cache agrees with what is restored at the end of the callback.
    XPLMSetGraphicsState(0, 1, 0, 0, 1, 0, 0);
    XPLMBindTexture2d(0, 0);
    scoped_gl_state restore;
    GLint viewport[4], outer_clip[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    if(glIsEnabled(GL_SCISSOR_TEST)) glGetIntegerv(GL_SCISSOR_BOX, outer_clip);
    else std::copy_n(viewport, 4, outer_clip);
    const auto setup = [] {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDisable(GL_CULL_FACE);
        glDisable(GL_STENCIL_TEST);
        glEnable(GL_SCISSOR_TEST);
        glEnable(GL_TEXTURE_2D);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glShadeModel(GL_SMOOTH);
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
        glEnableClientState(GL_VERTEX_ARRAY);
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        glEnableClientState(GL_COLOR_ARRAY);
    };
    setup();
    glMatrixMode(GL_MODELVIEW);
    glTranslatef(static_cast<float>(left), static_cast<float>(top), 0);
    glScalef(1, -1, 1);
    GLfloat model[16], projection[16];
    glGetFloatv(GL_MODELVIEW_MATRIX, model);
    glGetFloatv(GL_PROJECTION_MATRIX, projection);
    for(const auto * list : data.CmdLists) {
        for(const auto & cmd : list->CmdBuffer) {
            if(cmd.UserCallback) {
                if(cmd.UserCallback != ImDrawCallback_ResetRenderState) cmd.UserCallback(list, &cmd);
                setup();
                continue;
            }
            const auto clip = editor_scissor({cmd.ClipRect.x, cmd.ClipRect.y, cmd.ClipRect.z, cmd.ClipRect.w},
                                              model, projection, viewport, outer_clip);
            if(clip[2] == 0 || clip[3] == 0) continue;
            glScissor(clip[0], clip[1], clip[2], clip[3]);
            glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(cmd.GetTexID()));
            const auto * vertices = list->VtxBuffer.Data + cmd.VtxOffset;
            glVertexPointer(2, GL_FLOAT, sizeof(ImDrawVert), &vertices->pos);
            glTexCoordPointer(2, GL_FLOAT, sizeof(ImDrawVert), &vertices->uv);
            glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(ImDrawVert), &vertices->col);
            glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(cmd.ElemCount),
                sizeof(ImDrawIdx) == 2 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT,
                list->IdxBuffer.Data + cmd.IdxOffset);
        }
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
