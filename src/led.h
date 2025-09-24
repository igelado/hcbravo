#ifndef LED_H_
#define LED_H_

#include <tuple>
#include <cstdint>

static const size_t LED_NR_BANKS = 4;
static const size_t LED_NR_BITS = 8;
using led_id = std::tuple<uint8_t, uint8_t>;
struct led_state {
    uint8_t id;
    uint8_t banks[LED_NR_BANKS];
    uint8_t reserved[64 - LED_NR_BANKS];
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
static const led_id LED_ANC_ANTI_INCE = { 2, 2 };
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