// SPDX-License-Identifier: LGPL-2.1-only
//
// tests/XPSDK/XPLM/XPLMUtilities.h
// XPlane Plugin for HoneyComb Bravo Throttle Controller
//
// Copyright (C) 2005 Isaac Gelado

#ifndef XPLMUTILITIES_H_
#define XPLMUTILITIES_H_

#include <cstdio>

static inline
void XPLMDebugString(const char * message) noexcept {
    std::fputs(message, stderr);
}

#endif
