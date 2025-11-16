// SPDX-License-Identifier: LGPL-2.1-only
//
// src/error.h
// XPlane Plugin for HoneyComb Bravo Throttle Controller
//
// Copyright (C) 2005 Isaac Gelado



#ifndef ERROR_H_
#define ERROR_H_

#include <expected>
#include <iostream>

enum class error {
    not_detected,
    hid_error,
    api_command,
    api_menu,
    api_loop,

};

template<typename T>
using result_type = std::expected<T, error>;

static inline
std::ostream &
operator<<(std::ostream & os, const error & e) noexcept {
    switch(e) {
        case error::not_detected:
            os << "Controller not Detected";
            break;
        case error::hid_error:
            os << "HID Subsystem not Available";
            break;
        case error::api_command:
            os << "Failed to add Command to XPlane";
            break;
        case error::api_menu:
            os << "Failed to add Menu to XPlane";
            break;
        case error::api_loop:
            os << "Failed to add Callback to XPlane";
            break;
    }
    return os;
}

#endif