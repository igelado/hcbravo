// SPDX-License-Identifier: LGPL-2.1-only
//
// src/led.cpp
// XPlane Plugin for HoneyComb Bravo Throttle Controller
//
// Copyright (C) 2005 Isaac Gelado

#include "led.h"

#include <cstring>

led_state::led_state() noexcept 
{
    ::memset(reinterpret_cast<void *>(&u), 0, sizeof(u));
}

led_state::led_state(led_state && other) noexcept 
{
    ::memcpy(&u, &other.u, sizeof(u));
}