// SPDX-License-Identifier: LGPL-2.1-only
//
// src/led.cpp
// XPlane Plugin for HoneyComb Bravo Throttle Controller
//
// Copyright (C) 2005 Isaac Gelado

#include "led.h"

#include <cstring>
#include <cwchar>

namespace led {

led_state::led_state() noexcept :
    hid_(nullptr)
{
    ::memset(reinterpret_cast<void *>(&u), 0, sizeof(u));
}

led_state::led_state(led_state && other) noexcept :
    hid_(other.hid_),
    synchronized_(other.synchronized_),
    consecutive_failures_(other.consecutive_failures_),
    next_retry_(other.next_retry_)
{
    ::memcpy(&u, &other.u, sizeof(u));
    other.hid_ = nullptr;
}

void
led_state::attach(hid_device_ * hid) noexcept
{
    hid_ = hid;
    synchronized_ = false;
    consecutive_failures_ = 0;
    next_retry_ = {};
}

void
led_state::detach() noexcept
{
    hid_ = nullptr;
    synchronized_ = false;
    consecutive_failures_ = 0;
    next_retry_ = {};
}

void
led_state::update(const led_mask & mask)
{
    if(hid_ == nullptr || connection_lost()) {
        return;
    }

    if(synchronized_ && ::memcmp(u.state_.banks_, mask.banks_, sizeof(mask.banks_)) == 0) {
        return;
    }
    const auto now = std::chrono::steady_clock::now();
    if(now < next_retry_) {
        return;
    }

    hid_data next_state;
    ::memcpy(&next_state, &u.state_, sizeof(next_state));
    ::memcpy(next_state.banks_, mask.banks_, sizeof(mask.banks_));
    const int ret = hid_send_feature_report(hid_,
        reinterpret_cast<const unsigned char *>(&next_state), sizeof(next_state));
    if(ret != static_cast<int>(sizeof(next_state))) {
        char error_text[1024] {};
        auto error = hid_error(hid_);
        std::mbstate_t conversion {};
        if(error == nullptr || std::wcsrtombs(error_text, &error, sizeof(error_text) - 1, &conversion) == static_cast<size_t>(-1)) {
            ::strcpy(error_text, "no readable HID error text");
        }
        logger::info() << "HID LED write failed: report_id=" << static_cast<unsigned int>(next_state.id_)
                 << " banks=[" << mask.describe() << "] bytes=" << ret << "/" << sizeof(next_state)
                 << " error=" << error_text << "; retrying in 5 seconds";
        synchronized_ = false;
        next_retry_ = now + retry_interval;
        ++consecutive_failures_;
        return;
    }
    consecutive_failures_ = 0;
    ::memcpy(&u.state_, &next_state, sizeof(u.state_));
    synchronized_ = true;
}

void
led_state::turn_off()
{
    // Unload/shutdown may have no later flight-loop callback. Always allow
    // this final all-off attempt even if a previous write started a cooldown.
    next_retry_ = {};
    update(led_mask {});
}

#if !defined(NDEBUG)
void
led_state::dump_state() const noexcept
{
    led_mask cached;
    ::memcpy(cached.banks_, u.state_.banks_, sizeof(cached.banks_));
    logger::debug() << "[LED dump] HID available=" << (hid_ != nullptr)
        << " synchronized=" << synchronized_ << " report_id=" << static_cast<unsigned int>(u.state_.id_)
        << " cached banks=[" << cached.describe() << "] bytes=" << sizeof(u.state_);
}
#endif

} // namespace led
