// SPDX-License-Identifier: LGPL-2.1-only
#include "logger.h"
#include <XPLM/XPLMUtilities.h>

void hcbravo_log(const char * message) noexcept {
    XPLMDebugString(message);
}
