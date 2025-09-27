#ifndef STATE_H_
#define STATE_H_

#include <XPLM/XPLMDataAccess.h>
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
    commands::ptr_type cmds_;

    using profile_map_type = std::unordered_map<std::string, std::shared_ptr<profile>>;
    profile_map_type profile_map_;
    XPLMDataRef plane_icao_data_ref_;
    std::optional<profile::ptr_type> plane_;

    XPLMFlightLoopID flight_loop_;

    state() noexcept;

    static
    float
    flight_iteration(float last_call, float last_iter, int counter, void * ref) noexcept;

public:
    using ptr_type = std::unique_ptr<state>;

    inline state(state &&) noexcept = default;

    inline
    ~state() {
        if(this->flight_loop_ != nullptr) XPLMDestroyFlightLoop(this->flight_loop_);
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
};



#endif