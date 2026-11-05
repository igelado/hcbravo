// SPDX-License-Identifier: LGPL-2.1-only
#ifndef HCBRAVO_PROFILE_EDITOR_H
#define HCBRAVO_PROFILE_EDITOR_H

#include <array>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <yaml-cpp/yaml.h>

struct ImFont;
namespace conf { class key; class value; }

class profile_editor {
    std::filesystem::path directory_;
    std::function<bool(const char *)> find_dataref_;
    std::string dataref_source_ = "X-Plane";
    std::unordered_map<std::string, bool> dataref_status_;
    double next_dataref_check_ = 0;
    std::filesystem::path source_;
    struct profile_badge {
        std::filesystem::path path;
        std::string icao;
        std::string aircraft;
    };
    std::vector<profile_badge> profiles_;
    bool refresh_profiles_ = true;
    YAML::Node document_;
    std::string name_;
    std::vector<std::string> aircraft_names_;
    std::vector<std::string> icao_codes_;
    std::string message_;
    ImFont * annunciator_font_ = nullptr;
    struct dataref_mapping {
        std::string path;
        bool invert = false;
        std::optional<int> index;
        std::vector<std::string> values;
    };
    using datarefs = std::vector<dataref_mapping>;
    struct gear_mappings {
        datarefs safe;
        datarefs unsafe;
    };
    std::array<gear_mappings, 3> gear_datarefs_;
    std::array<datarefs, 14> annunciator_datarefs_;
    int selected_annunciator_ = 0;
    bool gear_selected_ = false;
    std::array<datarefs, 8> autopilot_modes_;
    std::array<datarefs, 5> autopilot_dials_;
    datarefs mach_dataref_;
    int selected_mode_ = -1;
    int selected_dial_ = 0;
    bool autopilot_selected_ = false;

    void refresh_profiles();
    void draw_profile_list(const std::filesystem::path & active_profile);
    void draw_annunciators();
    void draw_gear_panel(float width);
    void draw_gear_mappings();
    void draw_autopilot();
    bool dataref_exists(const std::string & path);
    void draw_dataref_warnings();
    void draw_datarefs(datarefs & mappings, bool compact = false, bool single = false);
    static dataref_mapping read_dataref(const conf::key & key);
    YAML::Node serialize_autopilot() const;
    static datarefs read_datarefs(const conf::value & mapping);
    static bool valid_datarefs(const datarefs & mappings);
    const char * draft_error() const;
    static YAML::Node serialize_datarefs(const datarefs & mappings);
    YAML::Node serialize_gear() const;
    YAML::Node serialize_annunciators() const;
    void open(const std::filesystem::path & profile);
    std::string serialize_profile(const std::string & name) const;
    void save();
    void make_copy();

public:
    static constexpr int initial_width = 1000;
    static constexpr int initial_height = 800;

    explicit profile_editor(std::filesystem::path directory,
                            bool (*find_dataref)(const char *) = nullptr);
    void load_fonts();
    void set_dataref_lookup(std::function<bool(const char *)> lookup, std::string source);
    bool draw(const std::filesystem::path & active_profile = {}, const char * lookup_status = nullptr);
};

#endif
