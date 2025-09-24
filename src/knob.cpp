#include "knob.h"

#include <XPLM/XPLMUtilities.h>

#include <expected>

struct descriptor {
    const char * path;
    const char * desc;
    XPLMCommandRef commands::* cmd;
};

descriptor commands::descriptors[] = {
    { "HCBravo/Alt", "Autopilot Altitude Knob", &commands::sel_alt_ },
    { "HCBravo/VS", "Autopilot Vertical Speed Knob", &commands::sel_vs_ },
    { "HCBravo/HDG", "Autopilot Heading Knob", &commands::sel_hdg_ },
    { "HCBravo/CRS", "Autopilot Course Knob", &commands::sel_crs_ },
    { "HCBravo/IAS", "Autopilot Indicated Air Speed Knob", &commands::sel_ias_ },
};

int
commands::ap_knob_select(XPLMCommandRef cmd, XPLMCommandPhase phase, void * ref) {
    commands * self = reinterpret_cast<commands *>(ref);

    if(cmd == self->sel_alt_) {
        XPLMDebugString("Altitude Selected");
        self->active_ = selector::alt;
    }
    else if(cmd == self->sel_vs_) {
        XPLMDebugString("Vertical Speed Selected");
        self->active_ = selector::vs;
    }
    else if(cmd == self->sel_hdg_) {
        XPLMDebugString("Heading Selected");
        self->active_ = selector::hdg;
    }
    else if(cmd == self->sel_crs_) {
        XPLMDebugString("Course Selected");
        self->active_ = selector::crs;
    }
    else if(cmd == self->sel_ias_) {
        XPLMDebugString("Indicated Air Speed Selected");
        self->active_ = selector::ias;
    }
    else { return 1; }
    return 0;
}

int
commands::ap_knob_up(XPLMCommandRef cmd, XPLMCommandPhase phase, void * ref) {
    return 0;
}

int
commands::ap_knob_down(XPLMCommandRef cmd, XPLMCommandPhase phase, void * ref) {
    return 0;
}



std::expected<commands::ptr_type, int>
commands::init() noexcept
{
    ptr_type ret(new commands());
    for(const auto & desc  : descriptors) {
        ret.get()->*desc.cmd = XPLMCreateCommand(desc.path, desc.desc);
        if(ret.get()->*desc.cmd == nullptr) {
            XPLMDebugString("Failed to register Selection Command");
            return std::unexpected(0);
        }
        XPLMRegisterCommandHandler(ret.get()->*desc.cmd, ap_knob_select, 1, reinterpret_cast<void *>(ret.get()));
    }
    ret->inc_ = XPLMCreateCommand("HCBravo/Inc", "Autopilot Knob Up");
    if(ret->inc_ == nullptr) {
        XPLMDebugString("Failed to register Inc Command");
        return std::unexpected(0);
    }
    ret->dec_ = XPLMCreateCommand("HCBravo/Dec", "Autopilot Knob Down");
    if(ret->dec_ == nullptr) {
        XPLMDebugString("Failed to register Dec Command");
        return std::unexpected(0);
    }

    XPLMRegisterCommandHandler(ret->inc_, ap_knob_up, 1, reinterpret_cast<void *>(ret.get()));
    XPLMRegisterCommandHandler(ret->dec_, ap_knob_down, 1, reinterpret_cast<void *>(ret.get()));
 
    return ret;
}

commands::~commands() noexcept {
    if(this->dec_ != nullptr) XPLMUnregisterCommandHandler(this->dec_, ap_knob_down, 1, this);
    if(this->inc_ != nullptr) XPLMUnregisterCommandHandler(this->dec_, ap_knob_up, 1, this);

    for(const auto & desc: descriptors) {
        if(this->*desc.cmd != nullptr) XPLMUnregisterCommandHandler(this->*desc.cmd, ap_knob_select, 1, this);
    }
}