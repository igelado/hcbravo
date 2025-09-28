#ifndef LED_H_
#define LED_H_

#include <XPLM/XPLMUtilities.h>

#include <optional>
#include <tuple>
#include <cstdint>

#include <hidapi.h>

#include "logger.h"

static const size_t LED_NR_BANKS = 4;
static const size_t LED_NR_BITS = 8;
using led_id = std::tuple<uint8_t, uint8_t>;

class led_mask {
    uint8_t banks_[LED_NR_BANKS];
    friend class led_state;
public:
    inline
    led_mask() noexcept {
        ::memset(banks_, 0, sizeof(banks_));
    }

    inline
    void update(const led_id & id, bool value) noexcept {
        if(value == true) {
            banks_[std::get<0>(id)] |= (1 << std::get<1>(id));
        }
    }

    inline
    void update(const led_id & id, const std::optional<bool> & value) noexcept {
        if(value.has_value()) update(id, value.value());
    }
};

class led_state {
private:
    struct state {
        uint8_t id_;
        uint8_t banks_[LED_NR_BANKS];
        uint8_t reserved_[64 - LED_NR_BANKS];
    };

    using buffer_type = uint8_t[sizeof(state)];

    union {
        state   state_;
        buffer_type  buffer_;
    } u;
    hid_device_ * hid_;
public:
    led_state() noexcept;
    led_state(led_state &&) noexcept;

#if !defined(NDEBUG)
    inline
    bool
    get_led(const led_id & id) const noexcept {
        return (u.state_.banks_[std::get<0>(id)] & (static_cast<uint8_t>(1) << std::get<1>(id))) != 0;
    }
#endif

    inline
    void
    update(const led_mask & mask) {
        size_t n;
        for(n = 0; n < LED_NR_BANKS; ++n) {
            if(u.state_.banks_[n] != mask.banks_[n]) break;
        }
        if(n == LED_NR_BANKS) return;
        ::memcpy(u.state_.banks_, mask.banks_, sizeof(mask.banks_));
        int ret = hid_send_feature_report(this->hid_, this->u.buffer_, sizeof(*this));
        if(ret < 0) {
            logger() << "Failed to update LED state";
        }
    }

};

// Upper button bar LEDs
static const led_id LED_AP_HDG = { 0, 0 };
static const led_id LED_AP_NAV = { 0, 1 };
static const led_id LED_AP_APR = { 0, 2 };
static const led_id LED_AP_REV = { 0, 3 };
static const led_id LED_AP_ALT = { 0, 4 };
static const led_id LED_AP_VS =  { 0, 5 };
static const led_id LED_AP_IAS = { 0, 6 };

static const led_id LED_AP =     { 0, 7 };

// Landing Gear LEDs
static const led_id LED_LDG_L_GREEN = { 1, 0 };
static const led_id LED_LDG_L_RED =   { 1, 1 };
static const led_id LED_LDG_N_GREEN = { 1, 2 };
static const led_id LED_LDG_N_RED =   { 1, 3 };
static const led_id LED_LDG_R_GREEN = { 1, 4 };
static const led_id LED_LDG_R_RED =   { 1, 5 };

// Status LEDs
static const led_id LED_ANC_MSTR_WARN = { 1, 6 };
static const led_id LED_ANC_ENG_FIRE  = { 1, 7 };
static const led_id LED_ANC_OIL       = { 2, 0 };
static const led_id LED_ANC_FUEL      = { 2, 1 };
static const led_id LED_ANC_ANTI_ICE  = { 2, 2 };
static const led_id LED_ANC_STARTER   = { 2, 3 };
static const led_id LED_ANC_APU       = { 2, 4 };
static const led_id LED_ANC_MSTR_CTN  = { 2, 5 };
static const led_id LED_ANC_VACUUM    = { 2, 6 };
static const led_id LED_ANC_HYD       = { 2, 7 };
static const led_id LED_ANC_AUX_FUEL  = { 3, 0 };
static const led_id LED_ANC_PRK_BRK   = { 3, 1 };
static const led_id LED_ANC_VOLTS     = { 3, 2 };
static const led_id LED_ANC_DOOR      = { 3, 3 };



#endif