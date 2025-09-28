// SPDX-License-Identifier: LGPL-2.1-only
//
// src/profile-data-ref-impl.h
// XPlane Plugin for HoneyComb Bravo Throttle Controller
//
// Copyright (C) 2005 Isaac Gelado

#ifndef PROFILE_DATA_REF_IMPL_H_
#define PROFILE_DATA_REF_IMPL_H_

#include "profile.h"

#include <XPLM/XPLMDataAccess.h>

#include <yaml.h>

#include <expected>
#include <optional>

template<typename>
class data_ref;

template<typename T>
std::expected<T, int>
base_data_ref::build(const YAML::Node & node) noexcept
{
    XPLMDataRef data_ref = nullptr;
    bool invert = false;
    std::optional<size_t> index = std::nullopt;

    if(node.IsMap()) {
        data_ref = XPLMFindDataRef(node["key"].as<std::string>().c_str());
        if(node["invert"]) invert = node["invert"].as<bool>();
        if(node["index"]) index = node["index"].as<size_t>();
    }
    else if(node.IsScalar()) {
        data_ref = XPLMFindDataRef(node.as<std::string>().c_str());
    }
    else {
        logger() << "Invalid node " << node;
        return std::unexpected(0);
    }
    return T(std::move(data_ref), invert, index);

}

template<>
class data_ref<bool> : public bool_data_ref {
protected:
    inline
    data_ref(XPLMDataRef && data_ref, bool invert,
            std::optional<size_t> index) noexcept :
        bool_data_ref(std::move(data_ref), invert, index)
    {}

    friend class base_data_ref;
public:
    data_ref(data_ref && other) noexcept = default;

    static inline
    std::expected<data_ref, int>
    build(const YAML::Node & node) noexcept {
        return base_data_ref::build<data_ref>(node);
    }

    bool is_set() const noexcept final {
        if(this->data_ref_ == nullptr) return false;
        int value = 0;
        if(this->index_) {
            XPLMGetDatavi(this->data_ref_, &value, this->index_.value(), 1);
        }
        else { 
            value = XPLMGetDatai(this->data_ref_);
        }
        return this->invert_ ? value == 0 : value != 0;
    }
};

template<>
class data_ref<int> : public bool_data_ref {
protected:
    std::vector<int> values_;

    inline
    data_ref(XPLMDataRef && data_ref, bool invert,
            std::optional<size_t> index) noexcept :
        bool_data_ref(std::move(data_ref), invert, index)
    {}

    friend class base_data_ref;
public:
    data_ref(data_ref && other) noexcept = default;

    static inline
    std::expected<data_ref, int>
    build(const YAML::Node & node) noexcept {
        auto ret = base_data_ref::build<data_ref>(node);
        if(ret.has_value()) {
            if(node.IsMap()) {
                for(const auto v : node["values"]) {
                    ret.value().values_.emplace_back(v.as<int>());
                }
            }
        }
        return ret;
    }

    bool is_set() const noexcept final {
        if(this->data_ref_ == nullptr) return false;
        int value = 0;
        if(this->index_) {
            XPLMGetDatavi(this->data_ref_, &value, this->index_.value(), 1);
        }
        else { 
            value = XPLMGetDatai(this->data_ref_);
        }
        // If not specific values are specified, we compare against 0
        if(this->values_.empty()) {
            return this->invert_ ? value == 0 : value != 0;
        }
        // When values are specified, we compare against the targets
        for(const auto & v : this->values_) {
            if(v == value) return this->invert_ ? false : true;
        }
        return this->invert_ ? true : false;
    }
};

template<>
class data_ref<float> : public bool_data_ref {
protected:
    std::vector<float> values_;

    inline
    data_ref(XPLMDataRef && data_ref, bool invert,
            std::optional<size_t> index) noexcept :
        bool_data_ref(std::move(data_ref), invert, index)
    {}

    friend class base_data_ref;
public:
    inline
    data_ref(data_ref && other) noexcept = default;

    static inline
    std::expected<data_ref, int>
    build(const YAML::Node & node) noexcept {
        auto ret = base_data_ref::build<data_ref>(node);
        if(ret.has_value()) {
            if(node.IsMap()) {
                for(const auto v : node["values"]) {
                    ret.value().values_.emplace_back(v.as<float>());
                }
            }
        }
        return ret;
    }

    data_ref &
    operator=(data_ref && other) noexcept = default;

    inline
    bool is_set() const noexcept final {
        float value = this->get();
        if(this->values_.empty()) {
            return this->invert_ ? this->get() == 0.0f : this->get() != 0.0f;
        }
        // When values are specified, we compare against the targets
        for(const auto & v : this->values_) {
            if(v == value) return this->invert_ ? false : true;
        }
        return this->invert_ ? true : false;
    }

    inline
    float get() const noexcept {
        if(this->data_ref_ == nullptr) return 0.0f;
        if(this->index_) {
            float ret;
            XPLMGetDatavf(this->data_ref_, &ret, this->index_.value(), 1);
            return ret;
        }
        return XPLMGetDataf(this->data_ref_);
    }

    inline
    void set(float value) const noexcept {
        if(this->data_ref_ == nullptr) return;
        XPLMSetDataf(this->data_ref_, value);
    }
};

#endif