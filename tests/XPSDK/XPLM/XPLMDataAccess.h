// SPDX-License-Identifier: LGPL-2.1-only
//
// tests/XPSDK/XPLM/XPLMDataAccess.h
// XPlane Plugin for HoneyComb Bravo Throttle Controller
//
// Copyright (C) 2005 Isaac Gelado

#ifndef _XPLMDataAccess_h_
#define _XPLMDataAccess_h_

#include <memory>
#include <string>
#include <unordered_map>

#include <iostream>

struct xplm_data_ref {
    std::string name;
    union {
        int i;
        float f;
    } value;

    inline
    xplm_data_ref(std::string && name, int value) noexcept :
        name(std::move(name)),
        value(value)
    {}

    inline
    const xplm_data_ref &
    operator=(xplm_data_ref && other) noexcept {
        this->name = std::move(other.name);
        this->value = std::move(other.value);
        return *this;
    }
};


using XPLMDataRef = std::unique_ptr<xplm_data_ref>;

static inline
XPLMDataRef XPLMFindDataRef(const char * key) noexcept {
    return std::make_unique<xplm_data_ref>(std::string(key), 0);
}

static inline
int XPLMGetDatai(const XPLMDataRef & data_ref) noexcept {
    return data_ref->value.i;
}

static inline
int XPLMGetDatavi(const XPLMDataRef & data_ref, int * out, int off, int size) noexcept {
    if(out != nullptr) *out = data_ref->value.i;
    return 1;
}

static inline
float XPLMGetDataf(const XPLMDataRef & data_ref) noexcept {
    return data_ref->value.f;
}

static inline
int XPLMGetDatavf(const XPLMDataRef & data_ref, float * out, int off, int size) noexcept {
    if(out != nullptr) *out = data_ref->value.f;
    return 1;
}



static inline
void XPLMSetDataf(const XPLMDataRef & data_ref, float value) noexcept {
    data_ref->value.f = value;
}

#endif