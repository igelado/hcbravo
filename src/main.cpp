#include <XPLM/XPLMDefs.h>
#include <XPLM/XPLMDataAccess.h>
#include <XPLM/XPLMPlugin.h>
#include <XPLM/XPLMUtilities.h>

#include <hidapi.h>
#include <yaml.h>

#include <memory>
#include <optional>
#include <tuple>
#include <unordered_map>

#include "led.h"
#include "profile.h"
#include "state.h"

static std::optional<state> plugin_state;

PLUGIN_API 
int
XPluginStart(char * name, char * sig, char * desc) {
    strncpy(name, "HCBravo", 256);
    strncpy(sig, "hc.bravo", 256);
    strncpy(desc, "Plugin for HoneyComb Bravo Quadrant", 256);

    XPLMDebugString("HCBravo Plugin Starts");
    auto state = state::init();
    if(state.has_value() == false) {
        return 0;
    }
    plugin_state.emplace(std::move(state.value()));

    XPLMDebugString("HCBravo Initialized");
    return 1;
}

PLUGIN_API
void
XPluginStop(void) {
    plugin_state = std::nullopt;
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
{
    switch(msg) {
        case XPLM_MSG_PLANE_LOADED:
            if(plugin_state.has_value()) plugin_state.value().load_plane();
            break;
        case XPLM_MSG_PLANE_UNLOADED:
            if(plugin_state.has_value()) plugin_state.value().unload_plane();
            break;
    }
}