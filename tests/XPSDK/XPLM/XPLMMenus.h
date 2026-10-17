// SPDX-License-Identifier: LGPL-2.1-only
#pragma once
#include <string>
#include <vector>
#include <unordered_map>

using XPLMMenuID = void *;
using XPLMMenuHandler_f = void (*)(void *, void *);
inline constexpr int xplm_Menu_Unchecked = 1;
inline constexpr int xplm_Menu_Checked = 2;

namespace xplm_test {
inline XPLMMenuHandler_f menu_handler = nullptr;
inline void * menu_refcon = nullptr;
inline int menu_token;
inline std::vector<std::string> menu_items;
inline std::unordered_map<int, int> menu_checks;
}

static inline XPLMMenuID XPLMFindPluginsMenu() { return &xplm_test::menu_token; }
static inline int XPLMAppendMenuItem(XPLMMenuID, const char * name, void *, int) {
    xplm_test::menu_items.emplace_back(name);
    return static_cast<int>(xplm_test::menu_items.size()) - 1;
}
static inline XPLMMenuID XPLMCreateMenu(const char *, XPLMMenuID, int,
    XPLMMenuHandler_f handler, void * refcon) {
    xplm_test::menu_handler = handler;
    xplm_test::menu_refcon = refcon;
    return &xplm_test::menu_token;
}
static inline void XPLMDestroyMenu(XPLMMenuID) {}
static inline void XPLMRemoveMenuItem(XPLMMenuID, int) {}
static inline void XPLMCheckMenuItem(XPLMMenuID, int index, int checked) { xplm_test::menu_checks[index] = checked; }
