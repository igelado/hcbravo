#ifndef STATE_H_
#define STATE_H_

#include <XPLM/XPLMDataAccess.h>

#include <expected>

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

    inline
    state(hid_device * hid, commands::ptr_type && commands);

public:
    inline state(state &&) noexcept = default;

    inline
    ~state() {
        if(this->hid_ != nullptr) hid_close(this->hid_);
    }

    static
    std::expected<state, int>
    init() noexcept;

    bool load_plane();

    inline
    void unload_plane() { plane_ = std::nullopt; }

    inline
    void set_led(const led_id & id) noexcept {
        this->leds_.banks[std::get<0>(id)] |= (static_cast<uint8_t>(1) << std::get<1>(id));
    }

    inline
    void clear_led(const led_id & id) noexcept {
        this->leds_.banks[std::get<0>(id)] &= ~(static_cast<uint8_t>(1) << std::get<1>(id));
    }

    inline
    void update_led_state() const {
        auto buffer = reinterpret_cast<unsigned char *>(const_cast<led_state *>(&this->leds_));
        int ret = hid_send_feature_report(this->hid_, buffer, sizeof(this->leds_));
        if(ret < 0) {
            XPLMDebugString("Failed to update LED state");
        }
    }
};



#endif