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
    logger::info() << "Detected Error: " << msg;
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
        logger::debug() << entry.first << (enabled ? " enabled" : " disabled");
        return;
    }
#endif
    switch(id) {
        case 0:
            logger::info() << "Reloading Aircraft Profiles";
            try {
                self->reload();
            }
            catch(const std::exception & ex) {
                logger::info() << "Failed to reload Aircraft Profiles: " << ex.what();
                break;
            }
            catch(...) {
                logger::info() << "Failed to reload Aircraft Profiles";
                break;
            }
            logger::debug() << "Setting Active Plane";
            self->request_load_plane();
            break;
#if !defined(NDEBUG)
        case 1:
            logger::info() << "Reloading All Plugins";
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
            logger::info() << "Unknown Menu ID #" << id;
            break;
    }
}

float
state::flight_iteration(float call, float iter, int counter, void * _this) noexcept
{
    state * self = reinterpret_cast<state *>(_this);
    if(self == nullptr) return 0;
    self->poll_hid();
    if(self->load_pending_) {
        self->load_pending_ = false;
        self->waiting_for_data_refs_ = !self->load_plane();
        if(self->waiting_for_data_refs_) {
            logger::info() << "Profile binding failed; LEDs inactive. Waiting for new DataRefs or HoneyComb Bravo > Reload Aircraft Profiles";
        }
    }
    if(self->plane_.has_value() == false) {
        // Keep polling for a missing device; otherwise sleep until rescheduled.
        return self->hid_ == nullptr
            ? std::chrono::duration<float>(hid_reconnect_interval).count() : 0;
    }
    const auto plane = self->plane_.value();
    led::led_mask mask;
    const auto & system = plane->system();
    const bool powered = system.volts();
    if(powered == false) {
        self->leds_.update(mask);
        return unpowered_interval;
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

    return powered_interval;
}

#if !defined(NDEBUG)
void
state::dump_led_state() const noexcept
{
    logger::debug() << "[LED dump] Aircraft '" << active_aircraft_name_ << "'";
    if(plane_) {
        const auto & plane = *plane_.value();
        plane.system().dump_state();
        if(plane.autopilot()) plane.autopilot()->mode().dump_state();
        else logger::debug() << "[State dump] autopilot=unconfigured";
        if(plane.annunciator()) plane.annunciator()->dump_state();
        else logger::debug() << "[State dump] annunciator=unconfigured";
    }
    else logger::debug() << "[LED dump] No active aircraft profile";
    leds_.dump_state();
}

void
state::dump_autopilot_state() const noexcept
{
    logger::debug() << "[Autopilot dump] Aircraft '" << active_aircraft_name_ << "'";
    if(!plane_) {
        logger::debug() << "[Autopilot dump] No active aircraft profile";
        return;
    }
    const auto & plane = *plane_.value();
    logger::debug() << "[Autopilot dump] LED power enabled=" << plane.system().volts();
    if(plane.autopilot()) plane.autopilot()->dump_state();
    else logger::debug() << "[Autopilot dump] No autopilot configured";
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
        logger::info() << "Failed to initialize Plugin State: " << ex.what();
        return std::unexpected(error::error::configuration);
    }
    catch(...) {
        logger::info() << "Failed to initialize Plugin State";
        return std::unexpected(error::error::configuration);
    }

    logger::debug() << "Initializing HID";
    int res = hid_init();
    if(res < 0) {
        logger::info() << "Failed to initialize HID";
        return std::unexpected(error::error::hid_error);
    }
    st->hid_initialized_ = true;
    if(!st->try_open_hid()) {
        logger::info() << "HoneyComb Bravo not detected; will retry every "
                 << std::chrono::duration_cast<std::chrono::seconds>(hid_reconnect_interval).count() << " seconds";
    }

    auto commands = knob::commands::init(*st);
    if(commands.has_value() == false) {
        logger::info() << "Failed to Register HoneyComb Bravo Commands";
        return std::unexpected(commands.error());
    }
    st->cmds_ = std::move(commands.value());

#if !defined(NDEBUG)
    logger::debug() << "Registering Error Handler";
    XPLMSetErrorCallback(&state::error_handler);
#endif

    logger::debug() << "Creating Menu Entries";
    auto plugins_menu = XPLMFindPluginsMenu();
    int item = XPLMAppendMenuItem(plugins_menu, "HoneyComb Bravo", nullptr, 1);
    if(item < 0) {
        logger::info() << "Failed to Create HoneyComb Bravo Menu Entry";
        return std::unexpected(error::error::api_menu);
    }
    st->menu_item_ = item;
    st->menu_ = XPLMCreateMenu("HoneyComb Bravo", plugins_menu, item, &state::menu_handler, st.get());
    if(st->menu_ == nullptr) {
        logger::info() << "Failed to Create HoneyComb Bravo Menu";
        return std::unexpected(error::error::api_menu);
    }
    if(XPLMAppendMenuItem(st->menu_, "Reload Aircraft Profiles", reinterpret_cast<void *>(0), 0) < 0) {
        logger::info() << "Failed to Create HoneyComb Bravo Menu (Reload Aircraft Profiles)";
        return std::unexpected(error::error::api_menu);
    }
#if !defined(NDEBUG)
    if(XPLMAppendMenuItem(st->menu_, "Reload All Plugins", reinterpret_cast<void *>(1), 0) < 0) {
        logger::info() << "Failed to Create HoneyComb Bravo Menu (Reload All Plugins)";
        return std::unexpected(error::error::api_menu);
    }
    if(XPLMAppendMenuItem(st->menu_, "Dump LED State", reinterpret_cast<void *>(2), 0) < 0 ||
       XPLMAppendMenuItem(st->menu_, "Dump Autopilot State", reinterpret_cast<void *>(3), 0) < 0) {
        logger::info() << "Failed to Create HoneyComb Bravo State Dump Menu Entries";
        return std::unexpected(error::error::api_menu);
    }
    for(size_t index = 0; index < diagnostic_menu_entries.size(); ++index) {
        const auto & entry = diagnostic_menu_entries[index];
        const int menu_index = XPLMAppendMenuItem(st->menu_, entry.first, reinterpret_cast<void *>(index + 4), 0);
        if(menu_index < 0) {
            logger::info() << "Failed to Create HoneyComb Bravo Menu (" << entry.first << ")";
            return std::unexpected(error::error::api_menu);
        }
        st->diagnostics_menu_items_[index] = menu_index;
        XPLMCheckMenuItem(st->menu_, menu_index, xplm_Menu_Unchecked);
    }
#endif

    logger::debug() << "Creating Flight Loop Logic";
    XPLMCreateFlightLoop_t fl_params = {
        .structSize = sizeof(XPLMCreateFlightLoop_t),
        .phase = xplm_FlightLoop_Phase_BeforeFlightModel,
        .callbackFunc = flight_iteration,
        .refcon = st.get(),
    };
    st->flight_loop_ = XPLMCreateFlightLoop(&fl_params);
    if(st->flight_loop_ == nullptr) {
        logger::info() << "Failed to Create Flight Loop";
        return std::unexpected(error::error::api_loop);
    }

    return st;
}

bool
state::try_open_hid() noexcept
{
    next_hid_open_ = std::chrono::steady_clock::now() + hid_reconnect_interval;
    hid_device * device = hid_open(0x294b, 0x1901, nullptr);
    if(device == nullptr) return false;
    hid_ = device;
    leds_.attach(device);
    logger::info() << "HoneyComb Bravo Throttle Detected";
    return true;
}

void
state::poll_hid() noexcept
{
    // Disabled plugins must not touch the device or poll for it.
    if(!hid_initialized_ || !enabled_) return;
    if(hid_ != nullptr) {
        if(!leds_.connection_lost()) return;
        logger::info() << "HoneyComb Bravo disconnected after repeated write failures; will retry";
        hid_close(hid_);
        hid_ = nullptr;
        leds_.detach();
        next_hid_open_ = std::chrono::steady_clock::now() + hid_reconnect_interval;
        return;
    }
    if(std::chrono::steady_clock::now() < next_hid_open_) return;
    try_open_hid();
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
        logger::info() << "Cannot read missing DataRef '" << label << "'";
        return false;
    }

    int ret = XPLMGetDatab(data_ref, buffer, 0, static_cast<int>(buffer_size - 1));
    if(ret < 0) {
        logger::info() << "Failed to read DataRef '" << label << "'";
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
    if(logger::diagnostics::enabled(logger::diagnostic_category::configuration)) logger::debug() << "[Configuration debug] Reading Plugin Configuration Files";
#endif
    auto id = XPLMGetMyID();
    static char path[256];
    path[0] = '\0';
    XPLMGetPluginInfo(id, nullptr, path, nullptr, nullptr);
    path[sizeof(path) - 1] = '\0';
    if(path[0] == '\0') {
        logger::info() << "Cannot determine plugin path";
        return;
    }
    XPLMExtractFileAndPath(path);
    auto config_file_path = std::filesystem::absolute(std::string(path) + "/../conf");
#if !defined(NDEBUG)
    if(logger::diagnostics::enabled(logger::diagnostic_category::configuration)) logger::debug() << "[Configuration debug] Reading Configurations from " << config_file_path;
#endif
    int index = 0;
    int total_conf_files = 0;
    do {
        int file_count = 0;
        int directory_status = XPLMGetDirectoryContents(
            config_file_path.string().c_str(),  index, files_buffer, FILES_BUFFER_SIZE,
            files_indexes, FILES_INDEX_SIZE, &total_conf_files, &file_count
        );
        // A status of 0 means the listing did not fit in the buffers; the rest is
        // fetched by advancing 'index'. Never trust the count beyond our own arrays.
        if(file_count > static_cast<int>(FILES_INDEX_SIZE)) file_count = static_cast<int>(FILES_INDEX_SIZE);
#if !defined(NDEBUG)
        if(logger::diagnostics::enabled(logger::diagnostic_category::configuration)) logger::debug() << "[Configuration debug] Read " << (index + file_count) << " file(s) from " << total_conf_files << " file(s)";
#endif
        for(auto n = 0; n < file_count; ++n) {
            if(files_indexes[n] == nullptr) continue;
            auto config_file = config_file_path / std::string(files_indexes[n]);
#if !defined(NDEBUG)
            if(logger::diagnostics::enabled(logger::diagnostic_category::configuration)) logger::debug() << "[Configuration debug] Found " << config_file << " in configuration file ( " << config_file.extension() << ")";
#endif
            if(config_file.extension() != ".yaml") continue;
#if !defined(NDEBUG)
            if(logger::diagnostics::enabled(logger::diagnostic_category::configuration)) logger::debug() << "[Configuration debug] Reading " << config_file;
#endif
            auto parsed = conf::configuration::read(config_file.string());
            if(parsed.has_value()) {
                auto configuration = std::make_shared<conf::configuration>(std::move(parsed.value()));
                for(const auto &aircraft : configuration->aircrafts()) {
                    auto ret = aircraft_configurations.emplace(aircraft, configuration);
                    if(ret.second == false) {
                        logger::info() << "Not using '" << configuration->name() << "' for '" << aircraft
                                 << "' because another configuration already exists";
                    }
                    else {
#if !defined(NDEBUG)
                        if(logger::diagnostics::enabled(logger::diagnostic_category::configuration)) logger::debug() << "[Configuration debug] Using '" << configuration->name() << "' for '" << aircraft << "'";
#endif
                    }
                }
                for(const auto &model : configuration->models()) {
                    auto ret = model_configurations.emplace(model, configuration);
                    if(ret.second == false) {
                        logger::info() << "Not using '" << configuration->name() << "' for ICAO '" << model
                                 << "' because another configuration already exists";
                    }
                    else {
#if !defined(NDEBUG)
                        if(logger::diagnostics::enabled(logger::diagnostic_category::configuration)) logger::debug() << "[Configuration debug] Using '" << configuration->name() << "' for ICAO '" << model << "'";
#endif
                    }
                }
            }
        }
        if(file_count <= 0) {
            if(index < total_conf_files) {
                logger::info() << "Stopping configuration reload because a truncated directory listing "
                          "did not advance (status " << directory_status << ", read " << index << " of " << total_conf_files << " file(s))";
            }
            break;
        }
        index += file_count;
    } while(index < total_conf_files);
    configuration_aircraft_map_.swap(aircraft_configurations);
    configuration_model_map_.swap(model_configurations);
#if !defined(NDEBUG)
    if(logger::diagnostics::enabled(logger::diagnostic_category::configuration)) logger::debug() << "[Configuration debug] Done loading plugin configuration";
#endif
}

bool
state::load_plane() noexcept
{
    static char icao_name[64];
    static char ui_name[256];
    read_data_ref_string(plane_icao_data_ref_, icao_name, sizeof(icao_name), plane_icao_label_);
    read_data_ref_string(plane_name_data_ref_, ui_name, sizeof(ui_name), plane_name_label_);
    logger::debug() << "Aircraft '" << ui_name << "' (" << icao_name << ")";

    // First try to get a match for the specific Aircraft
    auto configuration = configuration_aircraft_map_.find(ui_name);
    if(configuration != configuration_aircraft_map_.end()) {
        auto profile = profile::configuration::build(*configuration->second);
        if(!profile) {
            logger::info() << "Failed to build profile for '" << ui_name << "'";
            return false;
        }
        plane_.emplace(std::move(profile.value()));
        active_aircraft_name_ = ui_name;
        logger::info() << "Aircraft enabled: '" << ui_name << "' (" << icao_name
                 << ") using profile '" << configuration->second->name() << "'";
        return true;
    }

    logger::debug() << "Cannot find a profile for '" << ui_name
             << "'. Falling back to profile for ICAO '" << icao_name << "'";
    configuration = configuration_model_map_.find(icao_name);
    if(configuration != configuration_model_map_.end()) {
        auto profile = profile::configuration::build(*configuration->second);
        if(!profile) {
            logger::info() << "Failed to build profile for ICAO '" << icao_name << "'";
            return false;
        }
        plane_.emplace(std::move(profile.value()));
        active_aircraft_name_ = ui_name;
        logger::info() << "Aircraft enabled: '" << ui_name << "' (" << icao_name
                 << ") using profile '" << configuration->second->name() << "'";
        return true;
    }

    logger::info() << "Profile not found for aircraft '" << ui_name << "' (" << icao_name << ")";
    return false;
}

void
state::request_load_plane() noexcept
{
    if(!enabled_) return;
    unload_plane();
    load_pending_ = true;
    logger::debug() << "Deferring profile binding until the first flight-loop callback";
    XPLMScheduleFlightLoop(flight_loop_, -1.0f, 1);
}

void
state::data_refs_added() noexcept
{
    // Coalesce notifications while a load is already pending, and never
    // rebuild an active profile or revive an unloaded aircraft.
    if(!enabled_ || !waiting_for_data_refs_ || load_pending_) return;
    load_pending_ = true;
    logger::debug() << "New DataRefs registered after a failed load; scheduling profile binding on the next flight-loop callback";
    XPLMScheduleFlightLoop(flight_loop_, -1.0f, 1);
}

void
state::disable() noexcept
{
    if(!enabled_) return;
    enabled_ = false;
    unload_plane();
    // An interval of zero deactivates the flight loop.
    XPLMScheduleFlightLoop(flight_loop_, 0.0f, 1);
}

void
state::enable() noexcept
{
    if(enabled_) return;
    enabled_ = true;
    request_load_plane();
}

void
state::unload_plane() noexcept
{
    if(plane_.has_value()) {
        logger::info() << "Aircraft disabled: '" << active_aircraft_name_ << "'";
    }
    this->plane_ = std::nullopt;
    active_aircraft_name_.clear();
    load_pending_ = false;
    waiting_for_data_refs_ = false;

    // Turn off all lights
    this->leds_.turn_off();
}

} // namespace state
