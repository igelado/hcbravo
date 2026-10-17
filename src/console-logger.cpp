// SPDX-License-Identifier: LGPL-2.1-only
#include "logger.h"
#include <cstdio>

void hcbravo_log(const char * message) noexcept {
    std::fputs(message, stderr);
}
