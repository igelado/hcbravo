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

#include <expected>
#include <optional>
#include <string>

#include <hidapi.h>

#include "knob.h"
#include "led.h"
#include "profile.h"

class state {
    hid_device * hid_;
    led_state leds_;
    XPLMMenuID menu_;
    commands::ptr_type cmds_;

    using profile_map_type = std::unordered_map<std::string, std::shared_ptr<profile>>;
    profile_map_type profile_aircraft_map_;
    profile_map_type profile_model_map_;
    XPLMDataRef plane_icao_data_ref_;
    XPLMDataRef plane_name_data_ref_;
    std::optional<profile::ptr_type> plane_;

    XPLMFlightLoopID flight_loop_;

    state() noexcept;

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
    using ptr_type = std::unique_ptr<state>;

    inline state(state &&) noexcept = default;

    inline
    ~state() {
        if(this->flight_loop_ != nullptr) XPLMDestroyFlightLoop(this->flight_loop_);
        unload_plane();
        if(this->hid_ != nullptr) hid_close(this->hid_);
    }

    static
    std::expected<state::ptr_type, int>
    init() noexcept;

    void
    reload() noexcept;

    bool
    load_plane() noexcept;

    void
    unload_plane() noexcept;

    inline
    const std::optional<profile::ptr_type> &
    active_plane() const noexcept {
        return this->plane_;
    }

};



#endif