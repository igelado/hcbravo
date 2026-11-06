// SPDX-License-Identifier: LGPL-2.1-only
#include "directory-selector.h"
#include "profile-editor.h"
#include "directory-paths.h"

#include <algorithm>
#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

namespace {
bool parent_directory_control(bool list_entry = false) {
    const float font = ImGui::GetFontSize();
    const auto padding = ImGui::GetStyle().FramePadding;
    const auto origin = ImGui::GetCursorScreenPos();
    const float width = font + ImGui::GetStyle().ItemInnerSpacing.x + ImGui::CalcTextSize("..").x;
    const bool clicked = list_entry
        ? ImGui::Selectable("##parent-directory", false, 0, ImVec2(0, font))
        : ImGui::Button("##parent-directory", ImVec2(width + 2 * padding.x, font + 2 * padding.y));
    auto * draw = ImGui::GetWindowDrawList();
    const auto color = ImGui::GetColorU32(ImGuiCol_Text);
    const ImVec2 start(origin.x + (list_entry ? 0 : padding.x),
                       origin.y + (list_entry ? 0 : padding.y));
    // Bent parent-folder arrow, drawn without relying on a Unicode glyph.
    const float x = start.x + font * 0.35f;
    const float y = start.y + font * 0.2f;
    const ImVec2 stem[] = {{start.x + font * 0.85f, start.y + font * 0.8f},
                          {x, start.y + font * 0.8f}, {x, y}};
    const ImVec2 head[] = {{x - font * 0.22f, y + font * 0.22f}, {x, y},
                          {x + font * 0.22f, y + font * 0.22f}};
    draw->AddPolyline(stem, 3, color, 0, 1.5f);
    draw->AddPolyline(head, 3, color, 0, 1.5f);
    draw->AddText(ImVec2(start.x + font + ImGui::GetStyle().ItemInnerSpacing.x, start.y), color, "..");
    ImGui::SetItemTooltip("Parent directory");
    return clicked;
}
}

directory_selector::directory_selector(const profile_editor & editor)
    : path_(editor.directory().string()) {}

bool directory_selector::browse(const std::filesystem::path & directory) {
    try {
        const auto resolved = std::filesystem::canonical(resolve_directory_path(directory,
            browsing_.empty() ? std::filesystem::current_path() : browsing_));
        std::vector<std::filesystem::path> folders;
        for(const auto & entry : std::filesystem::directory_iterator(resolved)) {
            std::error_code error;
            if(entry.is_directory(error)) folders.push_back(entry.path());
        }
        std::sort(folders.begin(), folders.end());
        browsing_ = resolved;
        browse_path_ = resolved.string();
        folders_ = std::move(folders);
        error_.clear();
        return true;
    }
    catch(const std::exception & error) {
        error_ = error.what();
        return false;
    }
}

bool directory_selector::select(profile_editor & editor, const std::filesystem::path & directory) {
    try {
        editor.set_directory(resolve_directory_path(directory, editor.directory()));
        path_ = editor.directory().string();
        error_.clear();
        return true;
    }
    catch(const std::exception & error) {
        error_ = error.what();
        return false;
    }
}

void directory_selector::draw(profile_editor & editor) {
    ImGui::SetNextItemWidth((std::max)(120.0f, ImGui::GetContentRegionAvail().x - 250.0f));
    const bool enter = ImGui::InputText("##configuration-directory", &path_, ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::SetItemTooltip("Configuration directory");
    ImGui::SameLine();
    if(ImGui::Button("Open directory") || enter) select(editor, path_);
    ImGui::SameLine();
    if(ImGui::Button("Browse...")) {
        std::error_code error;
        const auto start = std::filesystem::is_directory(editor.directory(), error)
            ? editor.directory() : std::filesystem::current_path(error);
        browse(start);
        ImGui::OpenPopup("Choose configuration directory");
    }
    ImGui::TextDisabled("Save any edits before switching directories.");
    if(!error_.empty()) ImGui::TextWrapped("%s", error_.c_str());
    ImGui::SetNextWindowSize(ImVec2(600, 420), ImGuiCond_FirstUseEver);
    if(ImGui::BeginPopupModal("Choose configuration directory", nullptr, ImGuiWindowFlags_NoSavedSettings)) {
        ImGui::SetNextItemWidth(-60);
        const bool go = ImGui::InputText("##browse-path", &browse_path_, ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::SameLine();
        if(ImGui::Button("Go") || go) browse(browse_path_);
        const bool can_go_up = !browsing_.empty() && browsing_ != browsing_.root_path();
        ImGui::BeginDisabled(!can_go_up);
        if(parent_directory_control()) browse("..");
        ImGui::EndDisabled();
        std::filesystem::path navigate;
        if(ImGui::BeginChild("Folders", ImVec2(0, -100), ImGuiChildFlags_Borders)) {
            ImGui::BeginDisabled(!can_go_up);
            if(parent_directory_control(true)) navigate = browsing_.parent_path();
            ImGui::EndDisabled();
            for(const auto & folder : folders_) {
                const auto label = folder.filename().string();
                if(ImGui::Selectable(label.c_str())) navigate = folder;
            }
            if(folders_.empty()) ImGui::TextUnformatted("No subdirectories.");
        }
        ImGui::EndChild();
        if(!navigate.empty()) browse(navigate);
        if(!error_.empty()) ImGui::TextWrapped("%s", error_.c_str());
        ImGui::TextWrapped("Switching directories closes the current profile and discards unsaved edits.");
        if(ImGui::Button("Select this directory") && select(editor, browsing_)) ImGui::CloseCurrentPopup();
        ImGui::SameLine();
        if(ImGui::Button("Cancel")) { error_.clear(); ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }
}
