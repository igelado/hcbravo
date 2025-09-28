// SPDX-License-Identifier: LGPL-2.1-only
//
// src/knob.h
// XPlane Plugin for HoneyComb Bravo Throttle Controller
//
// Copyright (C) 2005 Isaac Gelado


#ifndef COMMAND_H_
#define COMMAND_H_

#include <XPLM/XPLMUtilities.h>

#include <chrono>
#include <expected>
#include <memory>

enum class selector : size_t {
    alt = 0,
    vs = 1,
    hdg = 2,
    crs = 3,
    ias = 4
};

struct descriptor;

class state;

class commands {
public:
    using ptr_type = std::unique_ptr<commands>;

    enum class dir : int { inc = 1, dec = -1 };
private:
    static descriptor descriptors[];

    template<dir Dir>
    static int
    ap_knob_update(void * ref) noexcept;

    static int
    ap_knob_select(XPLMCommandRef cmd, XPLMCommandPhase phase, void * ref) noexcept;

    static int
    ap_knob_up(XPLMCommandRef cmd, XPLMCommandPhase phase, void * ref) noexcept;

    static int
    ap_knob_down(XPLMCommandRef cmd, XPLMCommandPhase phase, void * ref) noexcept;


    const state & state_;

    XPLMCommandRef sel_alt_;
    XPLMCommandRef sel_vs_;
    XPLMCommandRef sel_hdg_;
    XPLMCommandRef sel_crs_;
    XPLMCommandRef sel_ias_;
    XPLMCommandRef inc_;
    XPLMCommandRef dec_;

    selector active_;
    std::chrono::steady_clock::time_point   last_cmd_;

    inline
    commands(const state & state) :
        state_(state),
        sel_alt_(nullptr),
        sel_vs_(nullptr),
        sel_hdg_(nullptr),
        sel_crs_(nullptr),
        sel_ias_(nullptr),
        inc_(nullptr),
        dec_(nullptr),
        last_cmd_(std::chrono::steady_clock::now())
    {}

public:
    ~commands() noexcept;

    static
    std::expected<ptr_type, int>
    init(const state & state) noexcept;

    inline
    const selector &
    active() const { return active_; }
};



#endif