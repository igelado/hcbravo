// SPDX-License-Identifier: LGPL-2.1-only
//
// src/main.cpp
// XPlane Plugin for HoneyComb Bravo Throttle Controller
//
// Copyright (C) 2005 Isaac Gelado

#include <XPLM/XPLMDefs.h>
#include <XPLM/XPLMDataAccess.h>
#include <XPLM/XPLMPlugin.h>
#include <XPLM/XPLMUtilities.h>

#include <hidapi.h>
#include <cstdint>
#include <memory>
#include <optional>
#include <tuple>
#include <unordered_map>

#include "led.h"
#include "logger.h"
#include "profile.h"
#include "state.h"

static std::optional<state::ptr_type> plugin_state;

PLUGIN_API 
int
XPluginStart(char * name, char * sig, char * desc) {
    strncpy(name, "HCBravo", 256);
    strncpy(sig, "hc.bravo", 256);
    strncpy(desc, "Plugin for HoneyComb Bravo Quadrant", 256);

    debug_logger() << "HCBravo Plugin Starts";
    XPLMEnableFeature("XPLM_USE_NATIVE_PATHS", 1);
    XPLMEnableFeature("XPLM_WANTS_DATAREF_NOTIFICATIONS", 1);
    auto state = state::init();
    if(state.has_value() == false) {
       logger() << "Failed to initialize Plugin: " << state.error();
       return 0;
    }
    plugin_state.emplace(std::move(state.value()));

    logger() << "Plugin loaded";
    return 1;
}

PLUGIN_API
void
XPluginStop(void) {
    plugin_state = std::nullopt;
    logger() << "Plugin unloaded";
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
    if(id != XPLM_PLUGIN_XPLANE) return;
    auto plane_index = static_cast<int>(reinterpret_cast<intptr_t>(param));
    switch(msg) {
        case XPLM_MSG_PLANE_LOADED:
            if(plane_index != 0) break;
            debug_logger() << "Loading new aircraft";
            if(plugin_state.has_value()) plugin_state.value()->request_load_plane();
            break;
        case XPLM_MSG_PLANE_UNLOADED:
            if(plane_index != 0) break;
            debug_logger() << "Unloading current aircraft";
            if(plugin_state.has_value()) plugin_state.value()->unload_plane();
            break;
        case XPLM_MSG_DATAREFS_ADDED:
            if(plugin_state.has_value()) plugin_state.value()->data_refs_added();
            break;
    }
}
