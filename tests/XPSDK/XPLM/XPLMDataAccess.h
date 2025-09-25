#ifndef _XPLMDataAccess_h_
#define _XPLMDataAccess_h_

#include <memory>
#include <string>
#include <unordered_map>

#include <iostream>

struct xplm_data_ref {
    std::string name;
    int value;

    inline
    xplm_data_ref(std::string && name, int value) noexcept :
        name(std::move(name)),
        value(value)
    {}
};


using XPLMDataRef = std::unique_ptr<xplm_data_ref>;

static inline
XPLMDataRef XPLMFindDataRef(const char * key) noexcept {
    return std::make_unique<xplm_data_ref>(std::string(key), 0);
}

static inline
int XPLMGetDatai(const XPLMDataRef & data_ref) noexcept {
    return data_ref->value;
}


#endif