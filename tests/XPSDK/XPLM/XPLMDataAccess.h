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
#include <algorithm>
#include <cstring>

enum XPLMDataTypeID {
    xplmType_Unknown = 0,
    xplmType_Int = 1,
    xplmType_Float = 2,
    xplmType_Double = 4,
    xplmType_FloatArray = 8,
    xplmType_IntArray = 16,
    xplmType_Data = 32,
};

namespace xplm_test {
struct data_ref_settings {
    XPLMDataTypeID type = xplmType_Int;
    bool available = true;
    int array_read_count = 1;
    double value = 0;
};
inline std::unordered_map<std::string, data_ref_settings> data_refs;
inline std::unordered_map<std::string, int> read_counts;
inline std::unordered_map<std::string, std::string> last_access;
inline std::unordered_map<std::string, std::string> strings;
}


struct xplm_data_ref {
    std::string name;
    struct {
        int i;
        float f;
    } value;

    XPLMDataTypeID type;
    int array_read_count = 1;

    inline
    xplm_data_ref(std::string && name, int value) noexcept :
        name(std::move(name)),
        value {value, static_cast<float>(value)},
        type(xplmType_Int)
    {}

    inline
    const xplm_data_ref &
    operator=(xplm_data_ref && other) noexcept {
        this->name = std::move(other.name);
        this->value = std::move(other.value);
        return *this;
    }
};


using XPLMDataRef = std::shared_ptr<xplm_data_ref>;

typedef struct XPLMDataRefInfo {
    size_t structSize;
    XPLMDataTypeID type;    
    const char * name;
} XPLMDataRefInfo_t;

static inline
void XPLMGetDataRefInfo(const XPLMDataRef & data_ref, XPLMDataRefInfo * info) noexcept {
    if(info == nullptr || data_ref == nullptr) return;
    info->name = data_ref->name.c_str();
    info->type = data_ref->type;
}

static inline
XPLMDataRef XPLMFindDataRef(const char * key) noexcept {
    const auto settings = xplm_test::data_refs.find(key);
    if(settings != xplm_test::data_refs.end() && !settings->second.available) return nullptr;
    auto result = std::make_shared<xplm_data_ref>(std::string(key), 0);
    if(settings != xplm_test::data_refs.end()) {
        result->type = settings->second.type;
        result->array_read_count = settings->second.array_read_count;
        result->value.f = static_cast<float>(settings->second.value);
        result->value.i = static_cast<int>(settings->second.value);
    }
    return result;
}

static inline
int XPLMGetDatai(const XPLMDataRef & data_ref) noexcept {
    ++xplm_test::read_counts[data_ref->name];
    xplm_test::last_access[data_ref->name] = "int";
    if((data_ref->type & xplmType_Int) == 0) return 0;
    return data_ref->value.i;
}

static inline
int XPLMGetDatavi(const XPLMDataRef & data_ref, int * out, int off, int size) noexcept {
    ++xplm_test::read_counts[data_ref->name];
    xplm_test::last_access[data_ref->name] = "int array";
    if((data_ref->type & xplmType_IntArray) == 0) return 0;
    if(data_ref->array_read_count != 1) return data_ref->array_read_count;
    if(out != nullptr) *out = data_ref->value.i;
    return 1;
}

static inline
float XPLMGetDataf(const XPLMDataRef & data_ref) noexcept {
    ++xplm_test::read_counts[data_ref->name];
    xplm_test::last_access[data_ref->name] = "float";
    if((data_ref->type & xplmType_Float) == 0) return 0;
    return data_ref->value.f;
}

static inline
int XPLMGetDatavf(const XPLMDataRef & data_ref, float * out, int off, int size) noexcept {
    ++xplm_test::read_counts[data_ref->name];
    xplm_test::last_access[data_ref->name] = "float array";
    if((data_ref->type & xplmType_FloatArray) == 0) return 0;
    if(data_ref->array_read_count != 1) return data_ref->array_read_count;
    if(out != nullptr) *out = data_ref->value.f;
    return 1;
}

static inline
void XPLMSetDataf(const XPLMDataRef & data_ref, float value) noexcept {
    xplm_test::last_access[data_ref->name] = "float write";
    if((data_ref->type & xplmType_Float) == 0) return;
    data_ref->value.f = value;
    data_ref->value.i = static_cast<int>(value);
}

static inline
int XPLMSetDatavf(const XPLMDataRef & data_ref, float * value, int off, int size) noexcept {
    xplm_test::last_access[data_ref->name] = "float array write";
    if((data_ref->type & xplmType_FloatArray) == 0) return 0;
    if(value != nullptr) {
        data_ref->value.f = *value;
        data_ref->value.i = static_cast<int>(*value);
    }
    return 1;
}

static inline
int XPLMGetDatab(const XPLMDataRef & data_ref, void * out, int off, int size) noexcept {
    const auto & value = xplm_test::strings[data_ref->name];
    const auto count = (std::min)(size, static_cast<int>(value.size()) - off);
    if(count > 0) std::memcpy(out, value.data() + off, count);
    return (std::max)(0, count);
}

#endif
