// SPDX-License-Identifier: LGPL-2.1-only
//
// src/state.h
// XPlane Plugin for HoneyComb Bravo Throttle Controller
//
// Copyright (C) 2005 Isaac Gelado

#ifndef STATE_H_
#define STATE_H_

#include <XPLM/XPLMDataAccess.h>
#include <XPLM/XPLMMenus.h>
#include <XPLM/XPLMProcessing.h>

#include <chrono>
#include <optional>
#include <array>
#include <memory>
#include <string>
#include <unordered_map>

#include <hidapi.h>

#include "error.h"
#include "conf.h"
#include "knob.h"
#include "led.h"
#include "profile.h"

namespace state {

class state {
    hid_device * hid_;
    bool hid_initialized_;
    led::led_state leds_;
    XPLMMenuID menu_;
    int menu_item_;
    knob::commands::ptr_type cmds_;

    using configuration_ptr = std::shared_ptr<conf::configuration>;
    using configuration_map_type = std::unordered_map<std::string, configuration_ptr>;
    configuration_map_type configuration_aircraft_map_;
    configuration_map_type configuration_model_map_;
    XPLMDataRef plane_icao_data_ref_;
    XPLMDataRef plane_name_data_ref_;
    std::optional<profile::configuration::ptr_type> plane_;
    std::string active_aircraft_name_;
    bool load_pending_ = false;
    bool waiting_for_data_refs_ = false;
    bool enabled_ = true;
#if !defined(NDEBUG)
    std::array<int, 3> diagnostics_menu_items_ {};

    void dump_led_state() const noexcept;
    void dump_autopilot_state() const noexcept;
#endif

    XPLMFlightLoopID flight_loop_;
    std::chrono::steady_clock::time_point next_hid_open_ {};

    state();

    // Open the Bravo when absent and drop it when it stopped answering.
    void poll_hid() noexcept;
    bool try_open_hid() noexcept;

    static
    void
    error_handler(const char * msg) noexcept;

    static
    void
    menu_handler(void * menu, void * item) noexcept;

    static
    float
    flight_iteration(float last_call, float last_iter, int counter, void * ref) noexcept;

public:
    // Delay between attempts to open a missing Bravo (adjustable for tests).
    static inline std::chrono::steady_clock::duration hid_reconnect_interval = std::chrono::seconds(5);

    using ptr_type = std::unique_ptr<state>;

    // Seconds between LED refreshes. Annunciators do not need per-frame
    // polling; the unpowered interval only has to notice power coming back.
    static constexpr float powered_interval = 0.1f;
    static constexpr float unpowered_interval = 0.25f;

    // Menu, command and flight-loop callbacks hold pointers to this object.
    state(const state &) = delete;
    state & operator=(const state &) = delete;
    state(state &&) = delete;
    state & operator=(state &&) = delete;

    ~state() noexcept;

    static
    error::result_type<state::ptr_type>
    init() noexcept;

    void
    reload();

    bool
    load_plane() noexcept;

    void
    request_load_plane() noexcept;

    void
    data_refs_added() noexcept;

    void
    unload_plane() noexcept;

    // Plugin Admin disable/enable: disable darkens the LEDs and stops the
    // flight loop; enable rebinds the current aircraft, which X-Plane will
    // not announce again. X-Plane loads plugins disabled, so XPluginStart
    // calls start_disabled() and the first XPluginEnable performs the bind.
    void
    start_disabled() noexcept { enabled_ = false; }

    void
    disable() noexcept;

    void
    enable() noexcept;

    inline
    const std::optional<profile::configuration::ptr_type> &
    active_plane() const noexcept {
        return this->plane_;
    }

};



} // namespace state

#endif
