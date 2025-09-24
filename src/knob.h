#ifndef COMMAND_H_
#define COMMAND_H_

#include <XPLM/XPLMUtilities.h>

#include <expected>
#include <memory>

enum class selector {
    alt,
    vs,
    hdg,
    crs,
    ias
};

struct descriptor;

class commands {
public:
    using ptr_type = std::unique_ptr<commands>;
private:
    static descriptor descriptors[];

    static int
    ap_knob_select(XPLMCommandRef cmd, XPLMCommandPhase phase, void * ref);

    static int
    ap_knob_up(XPLMCommandRef cmd, XPLMCommandPhase phase, void * ref);

    static int
    ap_knob_down(XPLMCommandRef cmd, XPLMCommandPhase phase, void * ref);

    XPLMCommandRef sel_alt_;
    XPLMCommandRef sel_vs_;
    XPLMCommandRef sel_hdg_;
    XPLMCommandRef sel_crs_;
    XPLMCommandRef sel_ias_;
    XPLMCommandRef inc_;
    XPLMCommandRef dec_;

    selector active_;

    inline
    commands() :
        sel_alt_(nullptr),
        sel_vs_(nullptr),
        sel_hdg_(nullptr),
        sel_crs_(nullptr),
        sel_ias_(nullptr),
        inc_(nullptr),
        dec_(nullptr)
    {}
public:
    ~commands() noexcept;

    static
    std::expected<ptr_type, int>
    init() noexcept;

    inline
    const selector &
    active() const { return active_; }
};



#endif