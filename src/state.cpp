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

#include <algorithm>
#include <exception>
#include <expected>
#include <filesystem>
#include <memory>
#include <string>
#include <tuple>

#include <hidapi.h>

#include "led.h"
#include "logger.h"
#include "state.h"

namespace state {

#if !defined(NDEBUG)
static constexpr std::array diagnostic_menu_entries {
    std::pair {"Knob Diagnostics", logger::diagnostic_category::knobs},
    std::pair {"DataRef Diagnostics", logger::diagnostic_category::datarefs},
    std::pair {"Configuration Diagnostics", logger::diagnostic_category::configuration}
};
#endif

void
state::error_handler(const char * msg) noexcept
{
    logger::logger() << "Detected Error: " << msg;
}

void
state::menu_handler(void * _this, void * item) noexcept
{
    state * self = reinterpret_cast<state *>(_this);
    size_t id = reinterpret_cast<size_t>(item);
#if !defined(NDEBUG)
    if(id >= 4 && id < 4 + diagnostic_menu_entries.size()) {
        const auto index = id - 4;
        const auto & entry = diagnostic_menu_entries[index];
        const bool enabled = logger::diagnostics::toggle(entry.second);
        XPLMCheckMenuItem(self->menu_, self->diagnostics_menu_items_[index],
                         enabled ? xplm_Menu_Checked : xplm_Menu_Unchecked);
        logger::debug_logger() << entry.first << (enabled ? " enabled" : " disabled");
        return;
    }
#endif
    switch(id) {
        case 0:
            logger::logger() << "Reloading Aircraft Profiles";
            try {
                self->reload();
            }
            catch(const std::exception & ex) {
                logger::logger() << "Failed to reload Aircraft Profiles: " << ex.what();
                break;
            }
            catch(...) {
                logger::logger() << "Failed to reload Aircraft Profiles";
                break;
            }
            logger::debug_logger() << "Setting Active Plane";
            self->request_load_plane();
            break;
#if !defined(NDEBUG)
        case 1:
            logger::logger() << "Reloading All Plugins";
            XPLMReloadPlugins();
            break;
        case 2:
            self->dump_led_state();
            break;
        case 3:
            self->dump_autopilot_state();
            break;
#endif
        default:
            logger::logger() << "Unknown Menu ID #" << id;
            break;
    }
}

float
state::flight_iteration(float call, float iter, int counter, void * _this) noexcept
{
    state * self = reinterpret_cast<state *>(_this);
    if(self == nullptr) return 0;
    if(self->load_pending_) {
        self->load_pending_ = false;
        self->waiting_for_data_refs_ = !self->load_plane();
        if(self->waiting_for_data_refs_) {
            logger::logger() << "Profile binding failed; LEDs inactive. Waiting for new DataRefs or HoneyComb Bravo > Reload Aircraft Profiles";
        }
    }
    if(self->plane_.has_value() == false) {
        return 0;
    }
    const auto plane = self->plane_.value();
    led::led_mask mask;
    const auto & system = plane->system();
    const bool powered = system.volts();
    if(powered == false) {
        self->leds_.update(mask);
        return -1.0;
    }

    if(plane->autopilot().has_value()) {
        const auto & ap = plane->autopilot().value().mode();
        const auto hdg = ap.hdg(), nav = ap.nav(), apr = ap.apr(), rev = ap.rev();
        const auto alt = ap.alt(), vs = ap.vs(), ias = ap.ias();
        const bool enabled = ap.ap();
        mask.update(led::LED_AP_HDG, hdg);
        mask.update(led::LED_AP_NAV, nav);
        mask.update(led::LED_AP_APR, apr);
        mask.update(led::LED_AP_REV, rev);
        mask.update(led::LED_AP_ALT, alt);
        mask.update(led::LED_AP_VS, vs);
        mask.update(led::LED_AP_IAS, ias);
        mask.update(led::LED_AP, enabled);
    }

    if(system.gear().has_value()) {
        const auto & gear = system.gear().value();
        const auto left = gear.left_state();
        const auto nose = gear.nose_state();
        const auto right = gear.right_state();
        mask.update(led::LED_LDG_L_GREEN, left == profile::gear_state::safe);
        mask.update(led::LED_LDG_L_RED, left == profile::gear_state::unsafe);
        mask.update(led::LED_LDG_N_GREEN, nose == profile::gear_state::safe);
        mask.update(led::LED_LDG_N_RED, nose == profile::gear_state::unsafe);
        mask.update(led::LED_LDG_R_GREEN, right == profile::gear_state::safe);
        mask.update(led::LED_LDG_R_RED, right == profile::gear_state::unsafe);
    }

    if(plane->annunciator().has_value()) {
        const auto & ann = plane->annunciator().value();
        mask.update(led::LED_ANC_MSTR_WARN, ann.master_warn());
        mask.update(led::LED_ANC_ENG_FIRE, ann.eng_fire());
        mask.update(led::LED_ANC_OIL, ann.oil_low());
        mask.update(led::LED_ANC_FUEL, ann.fuel_low());
        mask.update(led::LED_ANC_ANTI_ICE, ann.anti_ice());
        mask.update(led::LED_ANC_STARTER, ann.starter());
        mask.update(led::LED_ANC_APU, ann.apu());
        mask.update(led::LED_ANC_MSTR_CTN, ann.master_caution());
        mask.update(led::LED_ANC_VACUUM, ann.vacuum_low());
        mask.update(led::LED_ANC_HYD, ann.hydro_low());
        mask.update(led::LED_ANC_AUX_FUEL, ann.aux_fuel());
        mask.update(led::LED_ANC_PRK_BRK, ann.parking_brake());
        mask.update(led::LED_ANC_VOLTS, ann.volt_low());
        mask.update(led::LED_ANC_DOOR, ann.door_open());
    }
    self->leds_.update(mask);

    return -1.0;
}

#if !defined(NDEBUG)
void
state::dump_led_state() const noexcept
{
    logger::debug_logger() << "[LED dump] Aircraft '" << active_aircraft_name_ << "'";
    if(plane_) {
        const auto & plane = *plane_.value();
        plane.system().dump_state();
        if(plane.autopilot()) plane.autopilot()->mode().dump_state();
        else logger::debug_logger() << "[State dump] autopilot=unconfigured";
        if(plane.annunciator()) plane.annunciator()->dump_state();
        else logger::debug_logger() << "[State dump] annunciator=unconfigured";
    }
    else logger::debug_logger() << "[LED dump] No active aircraft profile";
    leds_.dump_state();
}

void
state::dump_autopilot_state() const noexcept
{
    logger::debug_logger() << "[Autopilot dump] Aircraft '" << active_aircraft_name_ << "'";
    if(!plane_) {
        logger::debug_logger() << "[Autopilot dump] No active aircraft profile";
        return;
    }
    const auto & plane = *plane_.value();
    logger::debug_logger() << "[Autopilot dump] LED power enabled=" << plane.system().volts();
    if(plane.autopilot()) plane.autopilot()->dump_state();
    else logger::debug_logger() << "[Autopilot dump] No autopilot configured";
}
#endif

error::result_type<state::ptr_type>
state::init() noexcept
{
    state::ptr_type st;
    try {
        st = state::ptr_type(new state());
    }
    catch(const std::exception & ex) {
        logger::logger() << "Failed to initialize Plugin State: " << ex.what();
        return std::unexpected(error::error::configuration);
    }
    catch(...) {
        logger::logger() << "Failed to initialize Plugin State";
        return std::unexpected(error::error::configuration);
    }

    logger::debug_logger() << "Initializing HID";
    int res = hid_init();
    if(res < 0) {
        logger::logger() << "Failed to initialize HID";
        return std::unexpected(error::error::hid_error);
    }
    st->hid_initialized_ = true;
    st->hid_ = hid_open(0x294b, 0x1901, nullptr);
    if(st->hid_ == nullptr) {
        logger::logger() << "Open HoneyComb Bravo Quadrant not Detected";
        return std::unexpected(error::error::not_detected);
    }
    logger::logger() << "HoneyComb Bravo Throttle Detected";
    st->leds_.hid_ = st->hid_;

    auto commands = knob::commands::init(*st);
    if(commands.has_value() == false) {
        logger::logger() << "Failed to Register HoneyComb Bravo Commands";
        return std::unexpected(commands.error());
    }
    st->cmds_ = std::move(commands.value());

#if !defined(NDEBUG)
    logger::debug_logger() << "Registering Error Handler";
    XPLMSetErrorCallback(&state::error_handler);
#endif

    logger::debug_logger() << "Creating Menu Entries";
    auto plugins_menu = XPLMFindPluginsMenu();
    int item = XPLMAppendMenuItem(plugins_menu, "HoneyComb Bravo", nullptr, 1);
    if(item < 0) {
        logger::logger() << "Failed to Create HoneyComb Bravo Menu Entry";
        return std::unexpected(error::error::api_menu);
    }
    st->menu_item_ = item;
    st->menu_ = XPLMCreateMenu("HoneyComb Bravo", plugins_menu, item, &state::menu_handler, st.get());
    if(st->menu_ == nullptr) {
        logger::logger() << "Failed to Create HoneyComb Bravo Menu";
        return std::unexpected(error::error::api_menu);
    }
    if(XPLMAppendMenuItem(st->menu_, "Reload Aircraft Profiles", reinterpret_cast<void *>(0), 0) < 0) {
        logger::logger() << "Failed to Create HoneyComb Bravo Menu (Reload Aircraft Profiles)";
        return std::unexpected(error::error::api_menu);
    }
#if !defined(NDEBUG)
    if(XPLMAppendMenuItem(st->menu_, "Reload All Plugins", reinterpret_cast<void *>(1), 0) < 0) {
        logger::logger() << "Failed to Create HoneyComb Bravo Menu (Reload All Plugins)";
        return std::unexpected(error::error::api_menu);
    }
    if(XPLMAppendMenuItem(st->menu_, "Dump LED State", reinterpret_cast<void *>(2), 0) < 0 ||
       XPLMAppendMenuItem(st->menu_, "Dump Autopilot State", reinterpret_cast<void *>(3), 0) < 0) {
        logger::logger() << "Failed to Create HoneyComb Bravo State Dump Menu Entries";
        return std::unexpected(error::error::api_menu);
    }
    for(size_t index = 0; index < diagnostic_menu_entries.size(); ++index) {
        const auto & entry = diagnostic_menu_entries[index];
        const int menu_index = XPLMAppendMenuItem(st->menu_, entry.first, reinterpret_cast<void *>(index + 4), 0);
        if(menu_index < 0) {
            logger::logger() << "Failed to Create HoneyComb Bravo Menu (" << entry.first << ")";
            return std::unexpected(error::error::api_menu);
        }
        st->diagnostics_menu_items_[index] = menu_index;
        XPLMCheckMenuItem(st->menu_, menu_index, xplm_Menu_Unchecked);
    }
#endif

    logger::debug_logger() << "Creating Flight Loop Logic";
    XPLMCreateFlightLoop_t fl_params = {
        .structSize = sizeof(XPLMCreateFlightLoop_t),
        .phase = xplm_FlightLoop_Phase_BeforeFlightModel,
        .callbackFunc = flight_iteration,
        .refcon = st.get(),
    };
    st->flight_loop_ = XPLMCreateFlightLoop(&fl_params);
    if(st->flight_loop_ == nullptr) {
        logger::logger() << "Failed to Create Flight Loop";
        return std::unexpected(error::error::api_loop);
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

static
bool
read_data_ref_string(XPLMDataRef data_ref, char * buffer, size_t buffer_size, const char * label) noexcept
{
    if(buffer_size == 0) return false;
    buffer[0] = '\0';

    if(data_ref == nullptr) {
        logger::logger() << "Cannot read missing DataRef '" << label << "'";
        return false;
    }

    int ret = XPLMGetDatab(data_ref, buffer, 0, static_cast<int>(buffer_size - 1));
    if(ret < 0) {
        logger::logger() << "Failed to read DataRef '" << label << "'";
        return false;
    }

    size_t written = (std::min)(static_cast<size_t>(ret), buffer_size - 1);
    buffer[written] = '\0';
    return true;
}

state::state() :
    hid_(nullptr),
    hid_initialized_(false),
    menu_(nullptr),
    menu_item_(-1),
    cmds_(nullptr),
    plane_icao_data_ref_(
        XPLMFindDataRef(plane_icao_label_)
    ),
    plane_name_data_ref_(
        XPLMFindDataRef(plane_name_label_)
    ),
    plane_(std::nullopt),
    flight_loop_(nullptr)
{
#if !defined(NDEBUG)
    logger::diagnostics::reset();
#endif
    this->reload();
}

state::~state() noexcept
{
    if(this->flight_loop_ != nullptr) {
        XPLMDestroyFlightLoop(this->flight_loop_);
        this->flight_loop_ = nullptr;
    }

    unload_plane();

    if(this->menu_ != nullptr) {
        XPLMDestroyMenu(this->menu_);
        this->menu_ = nullptr;
    }
    if(this->menu_item_ >= 0) {
        XPLMRemoveMenuItem(XPLMFindPluginsMenu(), this->menu_item_);
        this->menu_item_ = -1;
    }

#if !defined(NDEBUG)
    XPLMSetErrorCallback(nullptr);
#endif

    if(this->hid_ != nullptr) {
        hid_close(this->hid_);
        this->hid_ = nullptr;
        this->leds_.hid_ = nullptr;
    }
    if(this->hid_initialized_) {
        hid_exit();
        this->hid_initialized_ = false;
    }
}

void
state::reload()
{
    configuration_map_type aircraft_configurations;
    configuration_map_type model_configurations;

#if !defined(NDEBUG)
    if(logger::diagnostics::enabled(logger::diagnostic_category::configuration)) logger::debug_logger() << "[Configuration debug] Reading Plugin Configuration Files";
#endif
    auto id = XPLMGetMyID();
    static char path[256];
    path[0] = '\0';
    XPLMGetPluginInfo(id, nullptr, path, nullptr, nullptr);
    path[sizeof(path) - 1] = '\0';
    if(path[0] == '\0') {
        logger::logger() << "Cannot determine plugin path";
        return;
    }
    XPLMExtractFileAndPath(path);
    auto config_file_path = std::filesystem::absolute(std::string(path) + "/../conf");
#if !defined(NDEBUG)
    if(logger::diagnostics::enabled(logger::diagnostic_category::configuration)) logger::debug_logger() << "[Configuration debug] Reading Configurations from " << config_file_path;
#endif
    int index = 0;
    int total_conf_files = 0;
    do {
        int file_count = 0;
        int directory_status = XPLMGetDirectoryContents(
            config_file_path.string().c_str(),  index, files_buffer, FILES_BUFFER_SIZE,
            files_indexes, FILES_INDEX_SIZE, &total_conf_files, &file_count
        );
        if(directory_status == 0) {
            logger::logger() << "Configuration directory listing was truncated";
        }
#if !defined(NDEBUG)
        if(logger::diagnostics::enabled(logger::diagnostic_category::configuration)) logger::debug_logger() << "[Configuration debug] Read " << (index + file_count) << " file(s) from " << total_conf_files << " file(s)";
#endif
        for(auto n = 0; n < file_count; ++n) {
            if(files_indexes[n] == nullptr) continue;
            auto config_file = config_file_path / std::string(files_indexes[n]);
#if !defined(NDEBUG)
            if(logger::diagnostics::enabled(logger::diagnostic_category::configuration)) logger::debug_logger() << "[Configuration debug] Found " << config_file << " in configuration file ( " << config_file.extension() << ")";
#endif
            if(config_file.extension() != ".yaml") continue;
#if !defined(NDEBUG)
            if(logger::diagnostics::enabled(logger::diagnostic_category::configuration)) logger::debug_logger() << "[Configuration debug] Reading " << config_file;
#endif
            auto parsed = conf::configuration::read(config_file.string());
            if(parsed.has_value()) {
                auto configuration = std::make_shared<conf::configuration>(std::move(parsed.value()));
                for(const auto &aircraft : configuration->aircrafts()) {
                    auto ret = aircraft_configurations.emplace(aircraft, configuration);
                    if(ret.second == false) {
                        logger::logger() << "Not using '" << configuration->name() << "' for '" << aircraft
                                 << "' because another configuration already exists";
                    }
                    else {
#if !defined(NDEBUG)
                        if(logger::diagnostics::enabled(logger::diagnostic_category::configuration)) logger::debug_logger() << "[Configuration debug] Using '" << configuration->name() << "' for '" << aircraft << "'";
#endif
                    }
                }
                for(const auto &model : configuration->models()) {
                    auto ret = model_configurations.emplace(model, configuration);
                    if(ret.second == false) {
                        logger::logger() << "Not using '" << configuration->name() << "' for ICAO '" << model
                                 << "' because another configuration already exists";
                    }
                    else {
#if !defined(NDEBUG)
                        if(logger::diagnostics::enabled(logger::diagnostic_category::configuration)) logger::debug_logger() << "[Configuration debug] Using '" << configuration->name() << "' for ICAO '" << model << "'";
#endif
                    }
                }
            }
        }
        if(file_count <= 0) {
            if(index < total_conf_files) {
                logger::logger() << "Stopping configuration reload because directory iteration did not advance";
            }
            break;
        }
        index += file_count;
    } while(index < total_conf_files);
    configuration_aircraft_map_.swap(aircraft_configurations);
    configuration_model_map_.swap(model_configurations);
#if !defined(NDEBUG)
    if(logger::diagnostics::enabled(logger::diagnostic_category::configuration)) logger::debug_logger() << "[Configuration debug] Done loading plugin configuration";
#endif
}

bool
state::load_plane() noexcept
{
    static char icao_name[64];
    static char ui_name[256];
    read_data_ref_string(plane_icao_data_ref_, icao_name, sizeof(icao_name), plane_icao_label_);
    read_data_ref_string(plane_name_data_ref_, ui_name, sizeof(ui_name), plane_name_label_);
    logger::debug_logger() << "Aircraft '" << ui_name << "' (" << icao_name << ")";

    // First try to get a match for the specific Aircraft
    auto configuration = configuration_aircraft_map_.find(ui_name);
    if(configuration != configuration_aircraft_map_.end()) {
        auto profile = profile::configuration::build(*configuration->second);
        if(!profile) {
            logger::logger() << "Failed to build profile for '" << ui_name << "'";
            return false;
        }
        plane_.emplace(std::move(profile.value()));
        active_aircraft_name_ = ui_name;
        logger::logger() << "Aircraft enabled: '" << ui_name << "' (" << icao_name
                 << ") using profile '" << configuration->second->name() << "'";
        return true;
    }

    logger::debug_logger() << "Cannot find a profile for '" << ui_name
             << "'. Falling back to profile for ICAO '" << icao_name << "'";
    configuration = configuration_model_map_.find(icao_name);
    if(configuration != configuration_model_map_.end()) {
        auto profile = profile::configuration::build(*configuration->second);
        if(!profile) {
            logger::logger() << "Failed to build profile for ICAO '" << icao_name << "'";
            return false;
        }
        plane_.emplace(std::move(profile.value()));
        active_aircraft_name_ = ui_name;
        logger::logger() << "Aircraft enabled: '" << ui_name << "' (" << icao_name
                 << ") using profile '" << configuration->second->name() << "'";
        return true;
    }

    logger::logger() << "Profile not found for aircraft '" << ui_name << "' (" << icao_name << ")";
    return false;
}

void
state::request_load_plane() noexcept
{
    unload_plane();
    load_pending_ = true;
    logger::debug_logger() << "Deferring profile binding until the first flight-loop callback";
    XPLMScheduleFlightLoop(flight_loop_, -1.0f, 1);
}

void
state::data_refs_added() noexcept
{
    // Coalesce notifications while a load is already pending, and never
    // rebuild an active profile or revive an unloaded aircraft.
    if(!waiting_for_data_refs_ || load_pending_) return;
    load_pending_ = true;
    logger::debug_logger() << "New DataRefs registered after a failed load; scheduling profile binding on the next flight-loop callback";
    XPLMScheduleFlightLoop(flight_loop_, -1.0f, 1);
}


void
state::unload_plane() noexcept
{
    if(plane_.has_value()) {
        logger::logger() << "Aircraft disabled: '" << active_aircraft_name_ << "'";
    }
    this->plane_ = std::nullopt;
    active_aircraft_name_.clear();
    load_pending_ = false;
    waiting_for_data_refs_ = false;

    // Turn off all lights
    this->leds_.turn_off();
}

} // namespace state
