// SPDX-License-Identifier: LGPL-2.1-only
#pragma once
#include <cstring>
#include <string>

using XPLMPluginID = int;
inline constexpr XPLMPluginID XPLM_PLUGIN_XPLANE = 0;
inline constexpr int XPLM_MSG_PLANE_LOADED = 102;
inline constexpr int XPLM_MSG_PLANE_UNLOADED = 106;
inline constexpr int XPLM_MSG_DATAREFS_ADDED = 114;

static inline int XPLMGetMyID() { return 1; }
static inline void XPLMGetPluginInfo(int, char *, char * path, char *, char *) {
    const std::string plugin_path = std::string(HCBRAVO_SOURCE_DIR) + "/src/test.xpl";
    std::strcpy(path, plugin_path.c_str());
}
