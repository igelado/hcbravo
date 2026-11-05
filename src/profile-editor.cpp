// SPDX-License-Identifier: LGPL-2.1-only
#include "profile-editor.h"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <set>
#include <stdexcept>
#include <string_view>
#include <system_error>
#include <utility>

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include "editor-fonts.h"
#include "conf.h"

namespace {

void draw_match_list(const char * label, std::vector<std::string> & entries, bool required = false) {
    ImGui::PushID(label);
    ImGui::TextUnformatted(label);
    const float remove_width = ImGui::CalcTextSize("Remove").x +
                               2 * ImGui::GetStyle().FramePadding.x;
    auto remove = entries.size();
    for(size_t index = 0; index < entries.size(); ++index) {
        ImGui::PushID(static_cast<int>(index));
        ImGui::SetNextItemWidth(-(remove_width + ImGui::GetStyle().ItemSpacing.x));
        ImGui::InputText("##match", &entries[index]);
        ImGui::SameLine();
        ImGui::BeginDisabled(required && entries.size() == 1);
        if(ImGui::Button("Remove")) remove = index;
        ImGui::EndDisabled();
        ImGui::PopID();
    }
    if(remove < entries.size()) entries.erase(entries.begin() + remove);
    if(ImGui::Button("Add")) entries.emplace_back();
    ImGui::PopID();
}

bool valid_matches(const std::vector<std::string> & entries) {
    return !entries.empty() && std::none_of(entries.begin(), entries.end(),
                                            [](const auto & entry) { return entry.empty(); });
}

struct annunciator_label {
    const char * text;
    const char * yaml_key;
    const std::optional<conf::value> & (conf::annunciator::*mapping)() const;
};

constexpr annunciator_label annunciators[] = {
    {"MASTER\nWARNING", "master_warn", &conf::annunciator::master_warn},
    {"ENGINE\nFIRE", "eng_fire", &conf::annunciator::eng_fire},
    {"LOW OIL\nPRESSURE", "oil_low", &conf::annunciator::oil_low},
    {"LOW FUEL\nPRESSURE", "fuel_low", &conf::annunciator::fuel_low},
    {"ANTI\nICE", "anti_ice", &conf::annunciator::anti_ice},
    {"STARTER\nENGAGED", "starter", &conf::annunciator::starter},
    {"APU", "apu", &conf::annunciator::apu},
    {"MASTER\nCAUTION", "master_caution", &conf::annunciator::master_caution},
    {"VACUUM", "vacuum_low", &conf::annunciator::vacuum_low},
    {"LOW HYD\nPRESSURE", "hydro_low", &conf::annunciator::hydro_low},
    {"AUX FUEL\nPUMP", "aux_fuel", &conf::annunciator::aux_fuel},
    {"PARKING\nBRAKE", "parking_brake", &conf::annunciator::parking_brake},
    {"LOW\nVOLTS", "volt_low", &conf::annunciator::volt_low},
    {"DOOR", "door_open", &conf::annunciator::door_open}
};

constexpr const char * mode_labels[] = {"HDG", "NAV", "APR", "REV", "ALT", "VS", "IAS", "AUTO\nPILOT"};
constexpr const char * mode_keys[] = {"hdg", "nav", "apr", "rev", "alt", "vs", "ias", "ap"};
constexpr auto mode_accessors = std::array{
    &conf::autopilot_mode::hdg, &conf::autopilot_mode::nav, &conf::autopilot_mode::apr,
    &conf::autopilot_mode::rev, &conf::autopilot_mode::alt, &conf::autopilot_mode::vs,
    &conf::autopilot_mode::ias
};
constexpr const char * dial_labels[] = {"ALT", "VS", "HDG", "CRS", "IAS"};
constexpr const char * dial_keys[] = {"alt", "vs", "hdg", "crs", "ias"};
constexpr auto dial_accessors = std::array{
    &conf::autopilot_dial::alt, &conf::autopilot_dial::vs,
    &conf::autopilot_dial::heading, &conf::autopilot_dial::course
};

} // namespace

profile_editor::profile_editor(std::filesystem::path directory,
                               bool (*find_dataref)(const char *))
    : directory_(std::move(directory)), find_dataref_(find_dataref) {}

void profile_editor::load_fonts() {
    auto & io = ImGui::GetIO();
    auto * atlas = io.Fonts;
    ImFontConfig config;
    config.FontDataOwnedByAtlas = false;
    io.FontDefault = atlas->AddFontFromMemoryTTF(
        Roboto_Regular, sizeof(Roboto_Regular), 14.0f, &config);
    annunciator_font_ = atlas->AddFontFromMemoryTTF(
        Roboto_Medium, sizeof(Roboto_Medium), 14.0f, &config);
}

void profile_editor::draw_annunciators() {
    ImGui::TextWrapped("Select an annunciator or the gear panel to edit its DataRefs.");

    const auto origin = ImGui::GetCursorScreenPos();
    const float available = ImGui::GetContentRegionAvail().x;
    const float gear_width = std::min(136.0f, available / 3);
    const float width = available - gear_width - ImGui::GetStyle().ItemSpacing.x;
    constexpr float panel_height = 122.0f;
    constexpr float border = 5.0f;
    const float cell_width = (width - 2 * border) / 7;
    if(cell_width <= 0) return;
    constexpr float row_height = (panel_height - 2 * border) / 2;
    auto * font = annunciator_font_ ? annunciator_font_ : ImGui::GetFont();
    const float font_size = std::min(14.0f, cell_width / 5.9f);
    auto * draw = ImGui::GetWindowDrawList();

    const ImVec2 end(origin.x + width, origin.y + panel_height);
    draw->AddRectFilled(origin, end, IM_COL32(19, 20, 23, 255), 6.0f);
    draw->AddRect(origin, end, IM_COL32(68, 70, 74, 255), 6.0f);

    for(int row = 0; row < 2; ++row) {
        for(int column = 0; column < 7; ++column) {
            const auto index = row * 7 + column;
            const auto & datarefs = annunciator_datarefs_[index];
            const bool lit = std::any_of(datarefs.begin(), datarefs.end(),
                                         [](const auto & mapping) { return !mapping.path.empty(); });
            const char * label = annunciators[index].text;
            const float x = origin.x + border + (column + 0.5f) * cell_width;
            float y = origin.y + border + (row + 0.5f) * row_height;
            const ImVec2 cell_min(x - cell_width / 2, y - row_height / 2);
            const ImVec2 cell_max(x + cell_width / 2, y + row_height / 2);
            if(ImGui::IsWindowHovered() && ImGui::IsMouseHoveringRect(cell_min, cell_max)) {
                if(ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                    selected_annunciator_ = index;
                    gear_selected_ = false;
                }
                if(ImGui::BeginTooltip()) {
                    if(!lit) ImGui::TextUnformatted("No DataRef configured");
                    for(const auto & mapping : datarefs) {
                        if(mapping.path.empty()) continue;
                        if(mapping.index) ImGui::Text("%s[%d]", mapping.path.c_str(), *mapping.index);
                        else ImGui::TextUnformatted(mapping.path.c_str());
                    }
                    ImGui::EndTooltip();
                }
            }
            if(!gear_selected_ && index == selected_annunciator_) {
                draw->AddRect(cell_min, cell_max, IM_COL32(110, 140, 176, 255), 3.0f);
            }
            y -= std::strchr(label, '\n') ? font_size : font_size / 2;
            while(*label) {
                const char * end_of_line = std::strchr(label, '\n');
                if(end_of_line == nullptr) end_of_line = label + std::strlen(label);
                const auto size = font->CalcTextSizeA(font_size, cell_width, 0.0f, label, end_of_line);
                const ImVec2 position(x - size.x / 2, y);
                if(lit) {
                    for(const auto offset : {ImVec2(-1, 0), ImVec2(1, 0), ImVec2(0, -1), ImVec2(0, 1)}) {
                        draw->AddText(font, font_size, ImVec2(position.x + offset.x, position.y + offset.y),
                                      IM_COL32(255, 41, 16, 100), label, end_of_line);
                    }
                }
                draw->AddText(font, font_size, position,
                              lit ? IM_COL32(255, 198, 135, 255) : IM_COL32(154, 154, 154, 255),
                              label, end_of_line);
                label = *end_of_line ? end_of_line + 1 : end_of_line;
                y += font_size;
            }
        }
    }
    ImGui::Dummy(ImVec2(width, panel_height));
    ImGui::SameLine();
    draw_gear_panel(gear_width);
    if(gear_selected_) {
        draw_gear_mappings();
        return;
    }

    ImGui::PushID(selected_annunciator_);
    auto title = std::string(annunciators[selected_annunciator_].text);
    std::replace(title.begin(), title.end(), '\n', ' ');
    ImGui::TextUnformatted(title.c_str());
    draw_datarefs(annunciator_datarefs_[selected_annunciator_]);
    ImGui::PopID();
}

bool profile_editor::dataref_exists(const std::string & path) {
    if(!find_dataref_ || path.empty()) return false;
    auto [entry, inserted] = dataref_status_.try_emplace(path, false);
    if(inserted) entry->second = find_dataref_(path.c_str());
    return entry->second;
}

void profile_editor::set_dataref_lookup(std::function<bool(const char *)> lookup, std::string source) {
    find_dataref_ = std::move(lookup);
    dataref_source_ = std::move(source);
    dataref_status_.clear();
}

void profile_editor::draw_dataref_warnings() {
    if(!find_dataref_) return;

    // Retry periodically as aircraft and plugins can register DataRefs later.
    const double now = ImGui::GetTime();
    if(now >= next_dataref_check_) {
        dataref_status_.clear();
        next_dataref_check_ = now + 2.0;
    }

    std::set<std::string_view> missing;
    const auto check = [&](const datarefs & mappings) {
        for(const auto & mapping : mappings) {
            if(!mapping.path.empty() && !dataref_exists(mapping.path)) missing.insert(mapping.path);
        }
    };
    for(const auto & mappings : annunciator_datarefs_) check(mappings);
    for(const auto & gear : gear_datarefs_) {
        check(gear.safe);
        check(gear.unsafe);
    }
    for(const auto & mappings : autopilot_modes_) check(mappings);
    for(const auto & mappings : autopilot_dials_) check(mappings);
    check(mach_dataref_);

    if(missing.empty()) return;
    const auto title = "DataRef warnings (" + std::to_string(missing.size()) + ")###DataRef warnings";
    if(ImGui::CollapsingHeader(title.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
        if(ImGui::BeginChild("Missing DataRefs", ImVec2(0, 72), ImGuiChildFlags_Borders)) {
            for(const auto path : missing) {
                ImGui::TextWrapped("DataRef missing from %s: %.*s", dataref_source_.c_str(),
                                   static_cast<int>(path.size()), path.data());
            }
        }
        ImGui::EndChild();
    }
}

void profile_editor::draw_datarefs(datarefs & mappings, bool compact, bool single) {
    auto remove = mappings.size();
    bool add = false;
    const char * remove_label = compact ? "Remove" : "Remove DataRef";
    const char * add_label = compact ? "Add" : "Add DataRef";
    const auto draw_add = [&] {
        ImGui::SameLine();
        ImGui::BeginDisabled((single && !mappings.empty()) || !valid_datarefs(mappings));
        add = ImGui::Button(add_label);
        ImGui::EndDisabled();
    };
    if(mappings.empty()) {
        ImGui::BeginDisabled();
        ImGui::Button(remove_label);
        ImGui::EndDisabled();
        draw_add();
    }
    for(size_t index = 0; index < mappings.size(); ++index) {
        ImGui::PushID(static_cast<int>(index));
        auto & mapping = mappings[index];
        ImGui::Separator();
        const bool found = dataref_exists(mapping.path);
        if(found) ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(112, 224, 141, 255));
        if(compact) {
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::InputTextWithHint("##path", "DataRef path", &mapping.path);
        } else {
            ImGui::InputText("DataRef", &mapping.path);
        }
        if(found) ImGui::PopStyleColor();
        const bool options_open = !compact || ImGui::TreeNode("Options");
        if(options_open) {
            ImGui::Checkbox("Invert", &mapping.invert);
            bool indexed = mapping.index.has_value();
            if(ImGui::Checkbox("Use array index", &indexed)) {
                mapping.index = indexed ? std::optional<int>(0) : std::nullopt;
            }
            if(mapping.index) ImGui::InputInt("Index", &*mapping.index);
            draw_match_list("Values (optional)", mapping.values);
            if(compact) ImGui::TreePop();
        }
        if(ImGui::Button(remove_label)) remove = index;
        if(index + 1 == mappings.size()) draw_add();
        ImGui::PopID();
    }
    if(remove < mappings.size()) mappings.erase(mappings.begin() + remove);
    if(add) mappings.emplace_back();
}

namespace {

constexpr const char * gear_keys[] = {"left", "nose", "right"};

} // namespace

void profile_editor::draw_gear_panel(float width) {
    const auto origin = ImGui::GetCursorScreenPos();
    if(ImGui::InvisibleButton("Gear lights", ImVec2(width, 122.0f))) gear_selected_ = true;
    ImGui::SetItemTooltip("Edit gear DataRefs");

    const float scale = width / 136.0f;
    const float center = origin.x + width / 2;
    const float top = origin.y + (122.0f - 122.0f * scale) / 2;
    const auto point = [&](float x, float y) {
        return ImVec2(center + x * scale, top + y * scale);
    };
    auto * draw = ImGui::GetWindowDrawList();
    const ImVec2 bezel[] = {
        point(-16, 23), point(16, 23), point(54, 87),
        point(42, 107), point(-42, 107), point(-54, 87)
    };
    draw->AddConvexPolyFilled(bezel, 6, IM_COL32(26, 27, 30, 255));
    const ImU32 outline = gear_selected_ || ImGui::IsItemHovered()
        ? IM_COL32(110, 140, 176, 255) : IM_COL32(184, 187, 192, 255);
    draw->AddPolyline(bezel, 6, outline, ImDrawFlags_Closed, 2.0f);
    auto * font = annunciator_font_ ? annunciator_font_ : ImGui::GetFont();
    const auto label = [&](float x, float y, float size, const char * text) {
        const float font_size = size * scale;
        const auto bounds = font->CalcTextSizeA(font_size, width, 0.0f, text);
        auto position = point(x, y);
        position.x -= bounds.x / 2;
        draw->AddText(font, font_size, position, ImGui::GetColorU32(ImGuiCol_Text), text);
    };
    label(0, 5, 13, "N");
    label(-58, 102, 13, "L");
    label(58, 102, 13, "R");
    label(0, 63, 19, "GEAR");

    const auto has_path = [](const datarefs & entries) {
        return std::any_of(entries.begin(), entries.end(),
                           [](const auto & entry) { return !entry.path.empty(); });
    };
    const ImVec2 lamp_start[] = {
        point(-41, 78), point(-10, 34), point(41, 78)
    };
    const ImVec2 lamp_end[] = {
        point(-31, 95), point(10, 34), point(31, 95)
    };
    for(int index = 0; index < 3; ++index) {
        const auto & mappings = gear_datarefs_[index];
        const bool safe = has_path(mappings.safe);
        const bool unsafe = has_path(mappings.unsafe);
        ImU32 color = IM_COL32(55, 57, 61, 255);
        if(safe) color = unsafe ? IM_COL32(85, 232, 123, 255) : IM_COL32(255, 158, 56, 255);
        else if(unsafe) color = IM_COL32(244, 72, 67, 255);
        draw->AddLine(lamp_start[index], lamp_end[index], IM_COL32(8, 9, 11, 255), 11.0f * scale);
        draw->AddLine(lamp_start[index], lamp_end[index], color, 6.0f * scale);
    }
}

void profile_editor::draw_gear_mappings() {
    auto * font = annunciator_font_ ? annunciator_font_ : ImGui::GetFont();
    const auto title = [&](const char * text, float size) {
        ImGui::GetWindowDrawList()->AddText(font, size, ImGui::GetCursorScreenPos(),
                                           ImGui::GetColorU32(ImGuiCol_Text), text);
        ImGui::Dummy(font->CalcTextSizeA(size, ImGui::GetContentRegionAvail().x, 0.0f, text));
    };
    title("Gear mappings", 20.0f);
    ImGui::TextWrapped("Green: Both. Orange: Safe only. Red: Unsafe only. Off: Neither.");
    if(!ImGui::BeginTable("Gear mappings", 3, ImGuiTableFlags_BordersInnerV)) return;
    ImGui::TableNextRow();
    constexpr const char * labels[] = {"Left gear", "Nose gear", "Right gear"};
    for(int index = 0; index < 3; ++index) {
        ImGui::TableSetColumnIndex(index);
        ImGui::PushID(gear_keys[index]);
        auto & mappings = gear_datarefs_[index];
        title(labels[index], 16.0f);
        ImGui::PushID("safe");
        ImGui::TextUnformatted("Safe");
        draw_datarefs(mappings.safe, true);
        ImGui::PopID();
        ImGui::PushID("unsafe");
        ImGui::TextUnformatted("Unsafe");
        draw_datarefs(mappings.unsafe, true);
        ImGui::PopID();
        ImGui::PopID();
    }
    ImGui::EndTable();
}

void profile_editor::draw_autopilot() {
    const float spacing = ImGui::GetStyle().ItemSpacing.x;
    const float width = std::max(1.0f, (ImGui::GetContentRegionAvail().x - 9 * spacing) / 10);
    const ImVec2 size(width, 48.0f);
    const auto knob = [&](const char * label) {
        const auto origin = ImGui::GetCursorScreenPos();
        if(ImGui::InvisibleButton(label, size)) {
            selected_mode_ = -1;
            autopilot_selected_ = true;
        }
        const bool highlighted = ImGui::IsItemHovered() || (autopilot_selected_ && selected_mode_ < 0);
        const float radius = std::min(14.0f, width / 2);
        const ImVec2 center(origin.x + width / 2, origin.y + 17);
        auto * draw = ImGui::GetWindowDrawList();
        draw->AddCircleFilled(center, radius, IM_COL32(26, 27, 30, 255));
        draw->AddCircle(center, radius, ImGui::GetColorU32(highlighted ? ImGuiCol_ButtonHovered : ImGuiCol_Border));
        draw->AddLine(ImVec2(center.x, center.y - radius), ImVec2(center.x, center.y - radius / 2),
                      ImGui::GetColorU32(ImGuiCol_Text), 2.0f);
        const float text_width = ImGui::CalcTextSize(label).x;
        if(text_width <= width) {
            draw->AddText(ImVec2(center.x - text_width / 2, origin.y + 33),
                          ImGui::GetColorU32(ImGuiCol_Text), label);
        }
        ImGui::SetItemTooltip("%s dial", label);
    };
    knob("Selector");
    for(int index = 0; index < 8; ++index) {
        ImGui::SameLine();
        if(index == 7) {
            knob("Adjust");
            ImGui::SameLine();
        }
        const bool selected = autopilot_selected_ && selected_mode_ == index;
        const auto & mappings = autopilot_modes_[index];
        const bool configured = std::any_of(mappings.begin(), mappings.end(),
                                            [](const auto & mapping) { return !mapping.path.empty(); });
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, selected ? 2.0f : 1.0f);
        ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(12, 13, 15, 255));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(28, 30, 32, 255));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(5, 6, 8, 255));
        ImGui::PushStyleColor(ImGuiCol_Border, selected ? IM_COL32(113, 160, 213, 255)
                                                       : IM_COL32(60, 63, 67, 255));
        ImGui::PushStyleColor(ImGuiCol_Text, configured ? IM_COL32(193, 255, 160, 255)
                                                       : IM_COL32(146, 148, 151, 255));
        if(ImGui::Button(mode_labels[index], size)) {
            selected_mode_ = index;
            autopilot_selected_ = true;
        }
        ImGui::PopStyleColor(5);
        ImGui::PopStyleVar();
    }

    if(!autopilot_selected_) return;
    if(selected_mode_ >= 0) {
        ImGui::PushID(mode_keys[selected_mode_]);
        ImGui::Text("%s mode light DataRefs", selected_mode_ == 7 ? "Autopilot" : mode_labels[selected_mode_]);
        draw_datarefs(autopilot_modes_[selected_mode_]);
        ImGui::PopID();
        return;
    }

    ImGui::TextUnformatted("Dial target");
    for(int index = 0; index < 5; ++index) {
        if(index > 0) ImGui::SameLine();
        ImGui::RadioButton(dial_labels[index], &selected_dial_, index);
    }
    ImGui::PushID(dial_keys[selected_dial_]);
    ImGui::Text("%s value DataRef", dial_labels[selected_dial_]);
    draw_datarefs(autopilot_dials_[selected_dial_], false, true);
    if(selected_dial_ == 4) {
        ImGui::PushID("Mach");
        ImGui::TextUnformatted("Mach indicator DataRef");
        draw_datarefs(mach_dataref_, false, true);
        ImGui::PopID();
    }
    ImGui::PopID();
}

void profile_editor::refresh_profiles() {
    std::vector<profile_badge> profiles;
    for(const auto & entry : std::filesystem::directory_iterator(directory_)) {
        if(entry.path().extension() != ".yaml" || !entry.is_regular_file()) continue;
        const auto filename = entry.path().filename();
        const auto parsed = conf::configuration::read(entry.path().string());
        profiles.push_back({
            filename,
            parsed ? parsed->models().front() : "YAML",
            parsed ? parsed->aircrafts().front() : filename.generic_string()
        });
    }
    std::sort(profiles.begin(), profiles.end(), [](const auto & left, const auto & right) {
        return left.path < right.path;
    });
    profiles_ = std::move(profiles);
}

void profile_editor::draw_profile_list(const std::filesystem::path & active_profile) {
    const float heading_width = ImGui::CalcTextSize("Profiles").x +
                                ImGui::CalcTextSize("Refresh").x +
                                ImGui::GetStyle().ItemSpacing.x +
                                2 * ImGui::GetStyle().FramePadding.x;
    ImGui::TextUnformatted("Profiles");
    if(heading_width <= ImGui::GetContentRegionAvail().x) ImGui::SameLine();
    if(ImGui::SmallButton("Refresh")) refresh_profiles_ = true;
    if(refresh_profiles_) {
        refresh_profiles_ = false;
        refresh_profiles();
    }
    if(profiles_.empty()) ImGui::TextWrapped("No YAML profiles found.");

    constexpr float padding = 6.0f;
    for(const auto & profile : profiles_) {
        const auto path = profile.path.generic_string();
        ImGui::PushID(path.c_str());
        const auto origin = ImGui::GetCursorScreenPos();
        const float width = ImGui::GetContentRegionAvail().x;
        const float text_width = std::max(1.0f, width - 2 * padding);
        const auto name_size = ImGui::CalcTextSize(profile.aircraft.c_str(), nullptr, false, text_width);
        const float line_height = ImGui::GetTextLineHeight();
        const float height = name_size.y + line_height + 3 * padding;
        auto * draw = ImGui::GetWindowDrawList();
        draw->AddRectFilled(origin, ImVec2(origin.x + width, origin.y + height),
                            ImGui::GetColorU32(ImGuiCol_FrameBg), 5.0f);
        const bool selected = ImGui::Selectable("##profile", profile.path == source_, 0,
                                                 ImVec2(width, height));
        const ImVec2 code(origin.x + padding, origin.y + padding);
        const float code_width = ImGui::CalcTextSize(profile.icao.c_str()).x;
        draw->AddRectFilled(ImVec2(code.x - 3, code.y - 2),
                            ImVec2(code.x + code_width + 3, code.y + line_height + 2),
                            ImGui::GetColorU32(ImGuiCol_HeaderActive), 3.0f);
        const auto text_color = ImGui::GetColorU32(ImGuiCol_Text);
        draw->AddText(code, text_color, profile.icao.c_str());
        draw->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                      ImVec2(code.x, code.y + line_height + padding), text_color,
                      profile.aircraft.c_str(), nullptr, text_width);
        const bool active = profile.path == active_profile;
        if(active) {
            const ImVec2 light(origin.x + width - padding - 5.0f, code.y + line_height / 2);
            draw->AddCircleFilled(light, 7.0f, IM_COL32(76, 220, 113, 55));
            draw->AddCircleFilled(light, 4.0f, IM_COL32(102, 245, 137, 255));
        }
        ImGui::SetItemTooltip("%s%s", path.c_str(), active ? "\nActive in X-Plane" : "");
        ImGui::PopID();
        if(selected && profile.path != source_) open(profile.path);
    }
}

void profile_editor::open(const std::filesystem::path & profile) {
    const auto source = directory_ / profile;
    YAML::Node document;
    const auto parsed = conf::configuration::read(source.string(), &document);
    if(!parsed) {
        throw std::runtime_error("Could not open " + source.string() + ". See the log for details.");
    }
    annunciator_datarefs_ = {};
    if(const auto & panel = parsed->annunciator()) {
        for(size_t index = 0; index < annunciator_datarefs_.size(); ++index) {
            const auto & mapping = ((*panel).*annunciators[index].mapping)();
            if(!mapping) continue;
            annunciator_datarefs_[index] = read_datarefs(*mapping);
        }
    }
    gear_datarefs_ = {};
    if(const auto & gear = parsed->system().gear()) {
        gear_datarefs_[0] = {read_datarefs(gear->left().safe()), read_datarefs(gear->left().unsafe())};
        gear_datarefs_[1] = {read_datarefs(gear->nose().safe()), read_datarefs(gear->nose().unsafe())};
        gear_datarefs_[2] = {read_datarefs(gear->right().safe()), read_datarefs(gear->right().unsafe())};
    }
    autopilot_modes_ = {};
    autopilot_dials_ = {};
    mach_dataref_.clear();
    if(const auto & autopilot = parsed->autopilot()) {
        const auto & mode = autopilot->mode();
        for(size_t index = 0; index < mode_accessors.size(); ++index) {
            const auto & mapping = (mode.*mode_accessors[index])();
            if(mapping) autopilot_modes_[index] = read_datarefs(*mapping);
        }
        autopilot_modes_.back() = read_datarefs(mode.ap());
        if(const auto & dials = autopilot->dials()) {
            for(size_t index = 0; index < dial_accessors.size(); ++index) {
                const auto & mapping = ((*dials).*dial_accessors[index])();
                if(mapping) autopilot_dials_[index] = read_datarefs(*mapping);
            }
            if(const auto & ias = dials->ias()) {
                autopilot_dials_.back() = read_datarefs(ias->value());
                mach_dataref_.push_back(read_dataref(ias->is_mach()));
            }
        }
    }
    name_ = parsed->name();
    aircraft_names_ = parsed->aircrafts();
    icao_codes_ = parsed->models();
    document_ = document;
    source_ = profile;
    message_ = "Opened " + source.string();
}

YAML::Node profile_editor::serialize_annunciators() const {
    YAML::Node panel(YAML::NodeType::Map);
    for(size_t index = 0; index < annunciator_datarefs_.size(); ++index) {
        const auto & mappings = annunciator_datarefs_[index];
        if(mappings.empty()) continue;

        panel[annunciators[index].yaml_key] = serialize_datarefs(mappings);
    }
    return panel;
}

profile_editor::dataref_mapping profile_editor::read_dataref(const conf::key & key) {
    return {key.path(), key.inverted(), key.index(), key.values()};
}

profile_editor::datarefs profile_editor::read_datarefs(const conf::value & mapping) {
    datarefs result;
    for(const auto & key : mapping.keys()) {
        result.push_back(read_dataref(key));
    }
    return result;
}

bool profile_editor::valid_datarefs(const datarefs & mappings) {
    return std::all_of(mappings.begin(), mappings.end(), [](const auto & mapping) {
        return !mapping.path.empty() && (!mapping.index || *mapping.index >= 0);
    });
}

const char * profile_editor::draft_error() const {
    if(name_.empty()) return "Enter a profile name.";
    if(!valid_matches(aircraft_names_)) return "Enter at least one aircraft name and fill all name fields.";
    if(!valid_matches(icao_codes_)) return "Enter at least one ICAO code and fill all code fields.";
    const auto valid = [](const auto & groups) {
        return std::all_of(groups.begin(), groups.end(), valid_datarefs);
    };
    const bool valid_gear = std::all_of(gear_datarefs_.begin(), gear_datarefs_.end(), [](const auto & gear) {
        return valid_datarefs(gear.safe) && valid_datarefs(gear.unsafe);
    });
    if(!valid(annunciator_datarefs_) || !valid_gear || !valid(autopilot_modes_) ||
       !valid(autopilot_dials_) || !valid_datarefs(mach_dataref_)) {
        return "Fill every DataRef path and use nonnegative array indices.";
    }
    if(autopilot_dials_.back().empty() != mach_dataref_.empty()) {
        return "The IAS dial needs both value and Mach indicator DataRefs.";
    }
    const auto has_mapping = [](const auto & groups) {
        return std::any_of(groups.begin(), groups.end(), [](const auto & group) { return !group.empty(); });
    };
    if(autopilot_modes_.back().empty() && (has_mapping(autopilot_modes_) || has_mapping(autopilot_dials_))) {
        return "Add an AUTO PILOT DataRef before saving autopilot mappings.";
    }
    return nullptr;
}

YAML::Node profile_editor::serialize_datarefs(const datarefs & mappings) {
    YAML::Node sequence(YAML::NodeType::Sequence);
    for(const auto & mapping : mappings) {
        YAML::Node entry;
        entry["key"] = mapping.path;
        if(mapping.invert) entry["invert"] = true;
        if(mapping.index) entry["index"] = *mapping.index;
        if(!mapping.values.empty()) entry["values"] = mapping.values;
        sequence.push_back(entry);
    }
    return sequence;
}

YAML::Node profile_editor::serialize_gear() const {
    YAML::Node gear;
    for(size_t index = 0; index < gear_datarefs_.size(); ++index) {
        const auto & mappings = gear_datarefs_[index];
        YAML::Node signals(YAML::NodeType::Map);
        if(!mappings.safe.empty()) signals["safe"] = serialize_datarefs(mappings.safe);
        if(!mappings.unsafe.empty()) signals["unsafe"] = serialize_datarefs(mappings.unsafe);
        gear[gear_keys[index]] = signals;
    }
    return gear;
}

YAML::Node profile_editor::serialize_autopilot() const {
    YAML::Node modes(YAML::NodeType::Map);
    for(size_t index = 0; index < autopilot_modes_.size(); ++index) {
        if(!autopilot_modes_[index].empty()) {
            modes[mode_keys[index]] = serialize_datarefs(autopilot_modes_[index]);
        }
    }
    YAML::Node dials(YAML::NodeType::Map);
    for(size_t index = 0; index < dial_accessors.size(); ++index) {
        if(!autopilot_dials_[index].empty()) {
            dials[dial_keys[index]] = serialize_datarefs(autopilot_dials_[index])[0];
        }
    }
    const auto & ias = autopilot_dials_.back();
    if(!ias.empty() || !mach_dataref_.empty()) {
        dials["ias"]["value"] = serialize_datarefs(ias)[0];
        dials["ias"]["is_mach"] = serialize_datarefs(mach_dataref_)[0];
    }
    YAML::Node autopilot;
    if(modes.size() == 0 && dials.size() == 0) return autopilot;
    autopilot["modes"] = modes;
    if(dials.size() > 0) autopilot["dials"] = dials;
    return autopilot;
}

std::string profile_editor::serialize_profile(const std::string & name) const {
    if(const auto * error = draft_error()) throw std::runtime_error(error);
    auto document = YAML::Clone(document_);
    const auto panel = serialize_annunciators();
    const auto gear = serialize_gear();
    const auto autopilot = serialize_autopilot();
    const bool has_gear = std::any_of(gear_datarefs_.begin(), gear_datarefs_.end(),
                                     [](const auto & entry) { return !entry.safe.empty() || !entry.unsafe.empty(); });
    if(has_gear) document["system"]["gear"] = gear;
    else document["system"].remove("gear");
    if(panel.size() > 0) document["annunciator"] = panel;
    else document.remove("annunciator");
    if(autopilot.IsMap()) document["autopilot"] = autopilot;
    else document.remove("autopilot");
    document["name"] = name;
    document["aircrafts"] = aircraft_names_;
    document["models"] = icao_codes_;
    YAML::Emitter output;
    output << document;
    if(!output.good()) {
        throw std::runtime_error("Could not serialize the profile: " + output.GetLastError());
    }

    if(output.size() > conf::configuration::max_file_size) {
        throw std::runtime_error("The profile exceeds the 1 MiB configuration limit.");
    }

    return std::string(output.c_str(), output.size());
}

namespace {

bool write_new_profile(const std::filesystem::path & path, const std::string & contents) {
#ifdef _WIN32
    auto * file = _wfopen(path.c_str(), L"wbx");
#else
    auto * file = std::fopen(path.c_str(), "wbx");
#endif
    if(file == nullptr) {
        const auto error = errno;
        if(error == EEXIST) return false;
        throw std::system_error(error, std::generic_category(), "Could not create " + path.string());
    }
    const bool written = std::fwrite(contents.data(), 1, contents.size(), file) == contents.size();
    const bool closed = std::fclose(file) == 0;
    if(!written || !closed) {
        std::error_code ignored;
        std::filesystem::remove(path, ignored);
        throw std::runtime_error("Could not write " + path.string());
    }
    return true;
}

} // namespace

void profile_editor::save() {
    const auto contents = serialize_profile(name_);
    // Resolve symlinks before replacing the file so the profile link survives.
    const auto path = std::filesystem::canonical(directory_ / source_);
    const auto permissions = std::filesystem::status(path).permissions();
    std::filesystem::path temporary;
    for(unsigned int number = 1;; ++number) {
        temporary = path;
        temporary += ".save-" + std::to_string(number) + ".tmp";
        if(write_new_profile(temporary, contents)) break;
    }
    try {
        std::filesystem::permissions(temporary, permissions);
        std::filesystem::rename(temporary, path);
    }
    catch(...) {
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        throw;
    }
    message_ = "Saved " + path.string();
    refresh_profiles_ = true;
}

void profile_editor::make_copy() {
    auto copy_name = "Copy of " + name_;
    const auto contents = serialize_profile(copy_name);
    auto stem = source_.stem();
    stem += "-copy";
    for(unsigned int number = 1;; ++number) {
        auto filename = stem;
        if(number > 1) filename += "-" + std::to_string(number);
        filename += ".yaml";
        const auto path = directory_ / filename;
        if(!write_new_profile(path, contents)) continue;
        source_ = std::move(filename);
        name_ = std::move(copy_name);
        message_ = "Created " + path.string();
        refresh_profiles_ = true;
        return;
    }
}

bool profile_editor::draw(const std::filesystem::path & active_profile, const char * lookup_status) {
    bool close = false;
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    if(ImGui::Begin("Aircraft Profile Editor", nullptr,
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings)) {
        ImGui::TextUnformatted("Aircraft Profile Editor");
        ImGui::SameLine();
        close = ImGui::SmallButton("Close");
        ImGui::Separator();
        const float sidebar_width = std::min(220.0f, ImGui::GetContentRegionAvail().x * 0.34f);
        if(ImGui::BeginChild("Profile list", ImVec2(sidebar_width, 0), ImGuiChildFlags_Borders)) {
            try {
                draw_profile_list(active_profile);
            }
            catch(const std::exception & error) {
                message_ = error.what();
            }
        }
        ImGui::EndChild();
        ImGui::SameLine();
        if(ImGui::BeginChild("Profile details")) {
            try {
                if(!source_.empty()) {
                    ImGui::TextWrapped("Editing %s", source_.generic_string().c_str());
                    ImGui::InputText("Profile name", &name_);
                    const auto * error = draft_error();
                    ImGui::BeginDisabled(error != nullptr);
                    const bool save_requested = ImGui::Button("Save");
                    ImGui::EndDisabled();
                    if(error && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                        ImGui::SetTooltip("%s", error);
                    }
                    ImGui::SameLine();
                    ImGui::BeginDisabled(error != nullptr);
                    const bool copy_requested = ImGui::Button("Make copy");
                    ImGui::EndDisabled();
                    if(error && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                        ImGui::SetTooltip("%s", error);
                    }
                    if(save_requested) save();
                    if(copy_requested) make_copy();
                }
            }
            catch(const std::exception & error) {
                message_ = error.what();
            }
            ImGui::TextWrapped("%s", message_.c_str());
            if(lookup_status) ImGui::TextWrapped("%s", lookup_status);
            const bool has_profile = !source_.empty();
            if(has_profile) draw_dataref_warnings();
            else ImGui::TextWrapped("Select a profile on the left.");

            // Keep each editor visible and scroll long mappings inside their own pane.
            const float spacing = ImGui::GetStyle().ItemSpacing.y;
            const float pane_height = std::max(440.0f, ImGui::GetContentRegionAvail().y - 2 * spacing);
            if(ImGui::BeginChild("Aircraft matches pane", ImVec2(0, pane_height * 0.26f),
                                ImGuiChildFlags_Borders)) {
                ImGui::SeparatorText("Aircraft matches");
                if(has_profile) {
                    ImGui::TextWrapped("X-Plane checks aircraft names first, then ICAO codes.");
                    if(ImGui::BeginTable("Aircraft matches", 2, ImGuiTableFlags_SizingStretchSame)) {
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        draw_match_list("Aircraft names", aircraft_names_, true);
                        ImGui::TableSetColumnIndex(1);
                        draw_match_list("ICAO codes", icao_codes_, true);
                        ImGui::EndTable();
                    }
                }
            }
            ImGui::EndChild();
            if(ImGui::BeginChild("Annunciators pane", ImVec2(0, pane_height * 0.42f),
                                ImGuiChildFlags_Borders)) {
                ImGui::SeparatorText("Annunciators");
                if(has_profile) draw_annunciators();
            }
            ImGui::EndChild();
            if(ImGui::BeginChild("Autopilot pane", ImVec2(0, pane_height * 0.32f),
                                ImGuiChildFlags_Borders)) {
                ImGui::SeparatorText("Autopilot");
                if(has_profile) draw_autopilot();
            }
            ImGui::EndChild();
        }
        ImGui::EndChild();
    }
    ImGui::End();
    return close;
}
