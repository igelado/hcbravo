// SPDX-License-Identifier: LGPL-2.1-only
//
// src/state.cpp
// XPlane Plugin for HoneyComb Bravo Throttle Controller
//
// Copyright (C) 2005 Isaac Gelado

#include <XPLM/XPLMMenus.h>
#include <XPLM/XPLMPlugin.h>
#include <XPLM/XPLMProcessing.h>
#include <XPLM/XPLMUtilities.h>

#include <expected>
#include <filesystem>
#include <memory>
#include <string>
#include <tuple>

#include <hidapi.h>

#include "led.h"
#include "logger.h"
#include "state.h"

void
state::error_handler(const char * msg) noexcept
{
    logger() << "Detected Error: " << msg;
}

void
state::menu_handler(void * _this, void * item) noexcept
{
    state * self = reinterpret_cast<state *>(_this);
    size_t id = reinterpret_cast<size_t>(item);
    switch(id) {
        case 0:
            self->plane_ = std::nullopt;
            logger() << "Reloading Aircraft Profiles";
            self->reload();
            logger() << "Setting Active Plane";
            self->load_plane();
            break;
        case 1:
            logger() << "Reloading All Plugins";
            XPLMReloadPlugins();
            break;
        default:
            logger() << "Unknown Menu ID #" << id;
            break;
    }
}

float
state::flight_iteration(float call, float iter, int counter, void * _this) noexcept
{
    state * self = reinterpret_cast<state *>(_this);
    if(self == nullptr or self->plane_.has_value() == false) {
        logger() << "No active plane detected. Stopping Flight Loop Refresh";
        return 0;
    }
    const auto plane = self->plane_.value();

    led_mask mask;
    const auto & system = plane->system();
    if(system.volts() == false) return -1.0;

    if(plane->autopilot().has_value()) {
        const auto & ap = plane->autopilot().value().mode();
        mask.update(LED_AP_HDG, ap.hdg());
        mask.update(LED_AP_NAV, ap.nav());
        mask.update(LED_AP_APR, ap.apr());
        mask.update(LED_AP_REV, ap.rev());
        mask.update(LED_AP_ALT, ap.alt());
        mask.update(LED_AP_VS, ap.vs());
        mask.update(LED_AP_IAS, ap.ias());
        mask.update(LED_AP, ap.ap());
    }

    if(system.gear().has_value()) {
        bool status = system.gear().value();
        mask.update(LED_LDG_L_GREEN, status);
        mask.update(LED_LDG_L_RED, !status);

        mask.update(LED_LDG_N_GREEN, status);
        mask.update(LED_LDG_N_RED, !status);
    
        mask.update(LED_LDG_R_GREEN, status);
        mask.update(LED_LDG_R_RED, !status);
    }

    if(plane->annunciator().has_value()) {
        const auto & ann = plane->annunciator().value();
        mask.update(LED_ANC_MSTR_WARN, ann.master_warn());
        mask.update(LED_ANC_ENG_FIRE, ann.eng_fire());
        mask.update(LED_ANC_OIL, ann.oil_low());
        mask.update(LED_ANC_FUEL, ann.fuel_low());
        mask.update(LED_ANC_ANTI_ICE, ann.anti_ice());
        mask.update(LED_ANC_STARTER, ann.starter());
        mask.update(LED_ANC_APU, ann.apu());
        mask.update(LED_ANC_MSTR_CTN, ann.master_caution());
        mask.update(LED_ANC_VACUUM, ann.vacuum_low());
        mask.update(LED_ANC_HYD, ann.hydro_low());
        mask.update(LED_ANC_AUX_FUEL, ann.aux_fuel());
        mask.update(LED_ANC_PRK_BRK, ann.parking_brake());
        mask.update(LED_ANC_VOLTS, ann.volt_low());
        mask.update(LED_ANC_DOOR, ann.door_open());
    }
    self->leds_.update(mask);

    return -1.0;
}

std::expected<state::ptr_type, int>
state::init() noexcept
{
    state::ptr_type st = state::ptr_type(new state());

    logger() << "Initializing HID";
    int res = hid_init();
    if(res < 0) {
        logger() << "Failed to initialize HID";
        return std::unexpected(0);
    }
    st->hid_ = hid_open(0x294b, 0x1901, nullptr);
    if(st->hid_ == nullptr) {
        logger() << "Failed to Open HoneyComb Bravo Quadrant";
        return std::unexpected(0);
    }
    logger() << "HoneyComb Bravo Throttle Detected";
    st->leds_.hid_ = st->hid_;

    auto commands = commands::init(*st);
    if(commands.has_value() == false) {
        logger() << "Failed to Register HoneyComb Bravo Commands";
        return std::unexpected(commands.error());
    }
    st->cmds_ = std::move(commands.value());

    logger() << "Registering Error Handler";
    XPLMSetErrorCallback(&state::error_handler);

    logger() << "Creating Menu Entries";
    int item = XPLMAppendMenuItem(XPLMFindPluginsMenu(), "HoneyComb Bravo", nullptr, 1);
    st->menu_ = XPLMCreateMenu("HoneyComb Bravo", XPLMFindPluginsMenu(), item, &state::menu_handler, st.get());
    if(XPLMAppendMenuItem(st->menu_, "Reload Aircraft Profiles", reinterpret_cast<void *>(0), 0) < 0) {
        logger() << "Failed to Create HoneyComb Bravo Menu (Reload Aircraft Profiles)";
        return std::unexpected(0);
    }
    if(XPLMAppendMenuItem(st->menu_, "Reload All Plugins", reinterpret_cast<void *>(1), 0) < 0) {
        logger() << "Failed to Create HoneyComb Bravo Menu (Reload All Plugins)";
        return std::unexpected(0);
    }

    logger() << "Creating Flight Loop Logic";
    XPLMCreateFlightLoop_t fl_params = {
        .structSize = sizeof(XPLMCreateFlightLoop_t),
        .phase = xplm_FlightLoop_Phase_BeforeFlightModel,
        .callbackFunc = flight_iteration,
        .refcon = st.get(),
    };
    st->flight_loop_ = XPLMCreateFlightLoop(&fl_params);
    if(st->flight_loop_ == nullptr) {
        logger() << "Failed to Create Flight Loop";
        return std::unexpected(0);
    }

    return st;
}

static const size_t FILES_PATH_SIZE = 64;
static const size_t FILES_BUFFER_SIZE = 4096;
static const size_t FILES_INDEX_SIZE = FILES_BUFFER_SIZE / FILES_PATH_SIZE;
static char files_buffer[FILES_BUFFER_SIZE];
static char * files_indexes[FILES_INDEX_SIZE];

static const char * plane_icao_label_ = "sim/aircraft/view/acf_ICAO";
static const char * plane_name_label_ = "sim/aircraft/view/acf_ui_name";

state::state() noexcept :
    hid_(nullptr),
    menu_(nullptr),
    cmds_(nullptr),
    plane_icao_data_ref_(
        XPLMFindDataRef(plane_icao_label_)
    ),
    plane_name_data_ref_(
        XPLMFindDataRef(plane_name_label_)
    ),
    plane_(std::nullopt)
{
    memset(&this->leds_, 0, sizeof(led_state));
    this->reload();
}

void
state::reload() noexcept
{
    if(profile_aircraft_map_.empty() == false) { profile_aircraft_map_.clear(); }
    if(profile_model_map_.empty() == false) { profile_model_map_.clear(); }

    logger() << "Reading Plugin Configuration Files";
    auto id = XPLMGetMyID();
    static char path[256];
    XPLMGetPluginInfo(id, nullptr, path, nullptr, nullptr);
    XPLMExtractFileAndPath(path);
    auto config_file_path = std::filesystem::absolute(std::string(path) + "/../conf");
    logger() << "Reading Configurations from " << config_file_path;
    int index = 0;
    int total_conf_files = 0;
    do {
        int file_count = 0;
        XPLMGetDirectoryContents(
            config_file_path.string().c_str(),  index, files_buffer, FILES_BUFFER_SIZE,
            files_indexes, FILES_INDEX_SIZE, &total_conf_files, &file_count
        );
        logger() << "Read " << (index + file_count) << " file(s) from " << total_conf_files << " file(s)";
        for(auto n = 0; n < file_count; ++n) {
            auto config_file = config_file_path / std::string(files_indexes[n]);
            logger() << "Found " << config_file << " in configuration file ( " << config_file.extension() << ")";
            if(config_file.extension() != ".yaml") continue;
            logger() << "Reading " << config_file;
            auto prof = profile::from_yaml(config_file.string());
            if(prof.has_value()) {
                for(const auto &aircraft : prof.value()->aircrafts()) {
                    auto ret = profile_aircraft_map_.emplace(aircraft, prof.value());
                    if(ret.second == false) {
                        logger() << "Not using '" << prof.value()->name() << "' for '" << aircraft 
                                 << "' because another profile already exists";
                    }
                    else {
                        logger() << "Using '" << prof.value()->name() << "' for '" << aircraft << "'";
                    }
                }
                for(const auto &model : prof.value()->models()) {
                    auto ret = profile_model_map_.emplace(model, prof.value());
                    if(ret.second == false) {
                        logger() << "Not using '" << prof.value()->name() << "' for ICAO '" << model 
                                 << "' because another profile already exists";
                    }
                    else {
                        logger() << "Using '" << prof.value()->name() << "' for ICAO '" << model << "'";
                    }
                }
            }
        }
        index += file_count;
    } while(index != total_conf_files);
    logger() << "Done loading plugin configuration";
}

bool
state::load_plane() noexcept
{
    static char icao_name[64];
    static char ui_name[256];
    int ret = XPLMGetDatab(plane_icao_data_ref_, icao_name, 0, 64);
    if(ret < 64) icao_name[ret] = '\0';
    ret = XPLMGetDatab(plane_name_data_ref_, ui_name, 0, 256);
    if(ret < 256) ui_name[ret] = '\0';
    logger() << "Aircraft '" << ui_name << "' (" << icao_name << ")";

    // First try to get a match for the specific Aircraft
    auto profile = profile_aircraft_map_.find(ui_name);
    if(profile != profile_aircraft_map_.end()) {
        logger() << "Enabling profile '" << profile->first << "' for '" << ui_name << "'";
        plane_.emplace(profile->second);
        XPLMScheduleFlightLoop(this->flight_loop_, -1.0, 1);
        return true;
    }

    logger() << "Cannot find a profile for '" << ui_name 
             << "'. Falling back to profile for ICAO '" << icao_name << "'";
    profile = profile_model_map_.find(icao_name);
    if(profile != profile_model_map_.end()) {
        logger() << "Enabling profile '" << profile->first << "' for ICAO '" << icao_name << "'";
        plane_.emplace(profile->second);
        XPLMScheduleFlightLoop(this->flight_loop_, -1.0, 1);
        return true;
    }

    logger() << "Profile not found for aircraft '" << ui_name << "' (" << icao_name << ")";
    return false;
}


void
state::unload_plane() noexcept
{
    this->plane_ = std::nullopt;

    // Turn off all lights
    led_mask mask;
    this->leds_.update(mask);
}
