#include "knob.h"
#include "state.h"

#include <XPLM/XPLMUtilities.h>

#include <algorithm>
#include <expected>

#undef max

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

struct factor {
    const float slow;
    const float fast;
};

static const factor factors[] = {
    { 100.0f, 1000.0f },    // ALT
    { 100.0f, 100.0f },     // VS
    { 1.0f, 5.0f },     // HDG
    { 1.0f, 5.0f },     // CRS
    { 1.0f, 2.0f }      // IAS
};


int
commands::ap_knob_select(XPLMCommandRef cmd, XPLMCommandPhase phase, void * ref) noexcept
{
    commands * self = reinterpret_cast<commands *>(ref);

    if(cmd == self->sel_alt_) {
        logger() << "Altitude Selected";
        self->active_ = selector::alt;
    }
    else if(cmd == self->sel_vs_) {
        logger() << "Vertical Speed Selected";
        self->active_ = selector::vs;
    }
    else if(cmd == self->sel_hdg_) {
        logger() << "Heading Selected";
        self->active_ = selector::hdg;
    }
    else if(cmd == self->sel_crs_) {
        logger() << "Course Selected";
        self->active_ = selector::crs;
    }
    else if(cmd == self->sel_ias_) {
        logger() << "Indicated Air Speed Selected";
        self->active_ = selector::ias;
    }
    else { return 1; }
    return 0;
}




template<selector Sel, commands::dir Dir>
static inline
float
get_update_value(bool fast) noexcept {
    auto index = static_cast<size_t>(Sel);
    float value = fast ? factors[index].fast : factors[index].slow;
    return value * static_cast<int>(Dir);
}

template<commands::dir Dir>
static inline
int
commands::ap_knob_update(void * ref) noexcept
{
   commands * self = reinterpret_cast<commands *>(ref);

    // No plane is slected
    if(!self->state_.active_plane()) return 0;
    auto plane = self->state_.active_plane().value(); 

    // The plane has no autopilot 
    if(!plane->autopilot()) return 0;
    // The plance has no autopilot dials
    if(!plane->autopilot().value().dials()) return 0;

    const auto & dials = plane->autopilot().value().dials().value();

    auto now = std::chrono::steady_clock::now();
    auto elapsed = now - self->last_cmd_;
    // We set 250ms as threshold for now
    bool fast = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() < 250;
    switch(self->active_) {
        case selector::alt:
            if(!dials.alt()) return 0;
            dials.alt().value().set(
                std::max(
                    0.0f,
                    dials.alt().value().get() + get_update_value<selector::alt, dir::inc>(fast)
                )
            );
            break;

        case selector::vs:
            if(!dials.vs()) return 0;
            dials.vs().value().set(
                dials.alt().value().get() + get_update_value<selector::vs, dir::inc>(fast)
            );
            break;
        case selector::hdg:
            if(!dials.heading()) return 0;
            dials.heading().value().set(
                dials.heading().value().get() + get_update_value<selector::hdg, dir::inc>(fast)
            );
            break;
        case selector::crs:
            if(!dials.course()) return 0;
            dials.course().value().set(
                dials.course().value().get() + get_update_value<selector::crs, dir::inc>(fast)
            );
            break;
        case selector::ias:
            if(!dials.ias()) return 0;
            dials.ias().value().set(
                std::max(
                0.0f,
                dials.ias().value().get() + 
                    ((dials.ias().value().unit() == airspeed_unit::Knots) ?
                        get_update_value<selector::ias, dir::inc>(fast) : 
                        get_update_value<selector::ias, dir::inc>(fast) * 0.1f)
                )
            );
            break;
    }
    
    self->last_cmd_ = now;
    return 0;
}

int
commands::ap_knob_up(XPLMCommandRef cmd, XPLMCommandPhase phase, void * ref) noexcept
{
    return ap_knob_update<dir::inc>(ref);
}
 

int
commands::ap_knob_down(XPLMCommandRef cmd, XPLMCommandPhase phase, void * ref) noexcept
{
    return ap_knob_update<dir::dec>(ref);
}



std::expected<commands::ptr_type, int>
commands::init(const state & state) noexcept
{
    ptr_type ret(new commands(state));
    for(const auto & desc  : descriptors) {
        ret.get()->*desc.cmd = XPLMCreateCommand(desc.path, desc.desc);
        if(ret.get()->*desc.cmd == nullptr) {
            logger() << "Failed to register Selection Command";
            return std::unexpected(0);
        }
        XPLMRegisterCommandHandler(ret.get()->*desc.cmd, ap_knob_select, 1, reinterpret_cast<void *>(ret.get()));
    }
    ret->inc_ = XPLMCreateCommand("HCBravo/Inc", "Autopilot Knob Up");
    if(ret->inc_ == nullptr) {
        logger() << "Failed to register Inc Command";
        return std::unexpected(0);
    }
    ret->dec_ = XPLMCreateCommand("HCBravo/Dec", "Autopilot Knob Down");
    if(ret->dec_ == nullptr) {
        logger() << "Failed to register Dec Command";
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