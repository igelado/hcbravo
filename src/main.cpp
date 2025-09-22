#include <XPLM/XPLMDefs.h>
#include <XPLM/XPLMDataAccess.h>
#include <XPLM/XPLMPlugin.h>
#include <XPLM/XPLMUtilities.h>

#include <hidapi.h>
#include <yaml.h>

#include <tuple>

struct commands {
    XPLMCommandRef sel_alt;
    XPLMCommandRef sel_vs;
    XPLMCommandRef sel_hdg;
    XPLMCommandRef sel_crs;
    XPLMCommandRef sel_ias;
    XPLMCommandRef inc;
    XPLMCommandRef dec;

    inline
    commands() :
        sel_alt(nullptr),
        sel_vs(nullptr),
        sel_hdg(nullptr),
        sel_crs(nullptr),
        sel_ias(nullptr),
        inc(nullptr),
        dec(nullptr)
    {}
};

struct command {
    const char * path;
    const char * desc;
    XPLMCommandRef commands::* cmd;
};

static command sel_commands[] = {
    { "HCBravo/Alt", "Autopilot Altitude Knob", &commands::sel_alt },
    { "HCBravo/VS", "Autopilot Vertical Speed Knob", &commands::sel_vs },
    { "HCBravo/HDG", "Autopilot Heading Knob", &commands::sel_hdg },
    { "HCBravo/CRS", "Autopilot Course Knob", &commands::sel_crs },
    { "HCBravo/IAS", "Autopilot Indicated Air Speed Knob", &commands::sel_ias },
};

enum class selector {
    alt,
    vs,
    hdg,
    crs,
    ias
};

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

struct data_refs {
    XPLMDataRef hdg;
    XPLMDataRef nav;
    XPLMDataRef apr;
    XPLMDataRef rev;
    XPLMDataRef alt;
    XPLMDataRef vs;
    XPLMDataRef ias;
    XPLMDataRef gpss;

    XPLMDataRef ap;

    XPLMDataRef gear;

    XPLMDataRef mstr_warn;
    XPLMDataRef eng_fire;
    XPLMDataRef oil;
    XPLMDataRef fuel;
    XPLMDataRef anti_ice;
    XPLMDataRef starter;
    XPLMDataRef apu;
    XPLMDataRef mstr_ctn;
    XPLMDataRef vacuum;
    XPLMDataRef hyd;
    XPLMDataRef aux_fuel;
    XPLMDataRef prk_brk;
    XPLMDataRef volts;
    XPLMDataRef door;
};



struct plugin_state {
    hid_device * hid;
    led_state leds;
    commands cmd_ref;
    selector active;

    inline
    plugin_state() :
        hid(nullptr),
        active(selector::alt)
    {
        memset(&this->leds, 0, sizeof(led_state));
    }

    inline
    void set_led(const led_id & id) noexcept {
        this->leds.banks[std::get<0>(id)] |= (static_cast<uint8_t>(1) << std::get<1>(id));
    }

    inline
    void clear_led(const led_id & id) noexcept {
        this->leds.banks[std::get<0>(id)] &= ~(static_cast<uint8_t>(1) << std::get<1>(id));
    }

    inline
    void update_led_state() const {
        auto buffer = reinterpret_cast<unsigned char *>(const_cast<led_state *>(&this->leds));
        int ret = hid_send_feature_report(this->hid, buffer, sizeof(this->leds));
        if(ret < 0) {
            XPLMDebugString("Failed to update LED state");
        }
    }
};

static plugin_state state;


static int ap_knob_select(XPLMCommandRef cmd, XPLMCommandPhase phase, void * ref) {
    if(cmd == state.cmd_ref.sel_alt) {
        XPLMDebugString("Altitude Selected");
        state.active = selector::alt;
    }
    else if(cmd ==state.cmd_ref.sel_vs) {
        XPLMDebugString("Vertical Speed Selected");
        state.active = selector::vs;
    }
    else if(cmd == state.cmd_ref.sel_hdg) {
        XPLMDebugString("Heading Selected");
        state.active = selector::hdg;
    }
    else if(cmd == state.cmd_ref.sel_crs) {
        XPLMDebugString("Course Selected");
        state.active = selector::crs;
    }
    else if(cmd == state.cmd_ref.sel_ias) {
        XPLMDebugString("Indicated Air Speed Selected");
        state.active = selector::ias;
    }
    else { return 1; }
    return 0;
}

static int ap_knob_up(XPLMCommandRef cmd, XPLMCommandPhase phase, void * ref) {
    return 0;
}

static int
ap_knob_down(XPLMCommandRef cmd, XPLMCommandPhase phase, void * ref) {
    return 0;
}

static const size_t FILES_PATH_SIZE = 64;
static const size_t FILES_BUFFER_SIZE = 4096;
static char files_buffer[FILES_BUFFER_SIZE];
static char * files_indexes[FILES_BUFFER_SIZE / FILES_PATH_SIZE];

PLUGIN_API 
int
XPluginStart(char * name, char * sig, char * desc) {
    strncpy(name, "HCBravo", 256);
    strncpy(sig, "hc.bravo", 256);
    strncpy(desc, "Plugin for HoneyComb Bravo Quadrant", 256);

    XPLMDebugString("HCBravo Plugin Starts");

    XPLMDebugString("Initializing HID");
    int res = hid_init();
    if(res < 0) {
        XPLMDebugString("Failed to initialize HID");
        return 1;
    }
    auto hid_handle = hid_open(0x294b, 0x1901, nullptr);
    if(hid_handle == nullptr) {
        XPLMDebugString("Failed to Open HoneyComb Bravo Quadrant");
        return 1;
    }
    state.hid = hid_handle;

    XPLMDebugString("Reading Plugin Configuration Files");
    auto id = XPLMGetMyID();
    static char path[256];
    XPLMGetPluginInfo(id, nullptr, path, nullptr, nullptr);
    XPLMExtractFileAndPath(path);
    const char * sep = XPLMGetDirectorySeparator();
    std::string config_file_path = std::string(path).append(sep).append("config.yaml");
    YAML::Node config = YAML::LoadFile(config_file_path);
    if(config["profiles"]) {
        // Read all existing profiles
        for(const auto & profile : config["profiles"]) {
            std::string profile_path = std::string(path).append(sep).append(profile.as<std::string>());
            YAML::Node profile = YAML::LoadFile(profile_path);
        }
    }

    for(const auto & desc  : sel_commands) {
        state.cmd_ref.*desc.cmd = XPLMCreateCommand(desc.path, desc.desc);
        if(state.cmd_ref.*desc.cmd == nullptr) {
            XPLMDebugString("Failed to register Selection Command");
            return 0;
        }
        XPLMRegisterCommandHandler(state.cmd_ref.*desc.cmd, ap_knob_select, 1, nullptr);
    }
    state.cmd_ref.inc = XPLMCreateCommand("HCBravo/Inc", "Autopilot Knob Up");
    if(state.cmd_ref.inc == NULL) {
        XPLMDebugString("Failed to register Inc Command");
        return 0;
    }
    state.cmd_ref.dec = XPLMCreateCommand("HCBravo/Dec", "Autopilot Knob Down");
    if(state.cmd_ref.dec == NULL) {
        XPLMDebugString("Failed to register Dec Command");
        return 0;
    }

    XPLMRegisterCommandHandler(state.cmd_ref.inc, ap_knob_up, 1, nullptr);
    XPLMRegisterCommandHandler(state.cmd_ref.dec, ap_knob_down, 1, nullptr);
    XPLMDebugString("HCBravo Initialized");
    return 1;
}

PLUGIN_API
void
XPluginStop(void) {
    if(state.hid != nullptr) {
        hid_close(state.hid);
        state.hid = nullptr;
    }
}

PLUGIN_API
void
XPluginDisable(void) {}

PLUGIN_API
int
XPluginEnable(void) {
    return 1;
}

PLUGIN_API
void
XPluginReceiveMessage(XPLMPluginID id, int msg, void * param) 
{}