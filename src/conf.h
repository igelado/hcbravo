// SPDX-License-Identifier: LGPL-2.1-only
//
// src/conf.h
// XPlane Plugin for HoneyComb Bravo Throttle Controller
//
// Copyright (C) 2005 Isaac Gelado

#ifndef CONF_H_
#define CONF_H_

#include "logger.h"

#include <yaml.h>

#include <optional>
#include <expected>
#include <vector>

namespace conf {

class key {
protected:
    std::string key_;
    bool invert_;
    std::optional<int> index_;

    inline
    key(std::string && key, bool invert, std::optional<int> index) noexcept :
        key_(key),
        invert_(invert),
        index_(index)
    {}

public:
    static 
    std::expected<key, int>
    build(const YAML::Node & node);

    key(key && other) noexcept = default;

    key &
    operator=(key && other) noexcept = default;
};

class value {
protected:
    std::vector<key> keys_;

    inline
    value(std::vector<key> && keys) noexcept :
        keys_(std::move(keys))
    {}

public:
    static
    std::expected<value, int>
    build(const YAML::Node & node);

    inline
    const std::vector<key> &
    keys() const noexcept { return keys_; }

    inline
    bool
    empty() const noexcept { return this->keys_.empty(); }
};

class airspeed {
protected:
    key is_mach_;
    value value_;

    inline
    airspeed(key && is_mach, value && value) noexcept :
        is_mach_(std::move(is_mach)),
        value_(std::move(value))
    {}
public:

    static
    std::expected<airspeed, int>
    build(const YAML::Node & node);

};

class autopilot_dial {
protected:
    std::optional<airspeed> ias_;
    std::optional<value> course_;
    std::optional<value> heading_;
    std::optional<value> vs_;
    std::optional<value> alt_;

    inline
    autopilot_dial(
        std::optional<airspeed> && ias,
        std::optional<value> && course,
        std::optional<value> && heading,
        std::optional<value> && vs,
        std::optional<value> && alt
    ) noexcept :
        ias_(std::move(ias)),
        course_(std::move(course)),
        heading_(std::move(heading)),
        vs_(std::move(vs)),
        alt_(std::move(alt))
    {}

public:
    static
    std::expected<autopilot_dial, int>
    build(const YAML::Node & node);

    inline
    const std::optional<airspeed> &
    ias() const { return this->ias_; }

    inline
    const std::optional<value> &
    course() const { return this->course_; }

    inline
    const std::optional<value> &
    heading() const { return this->heading_; }

    inline
    const std::optional<value> &
    vs() const { return this->vs_; }

    inline
    const std::optional<value> &
    alt() const { return this->alt_; }
};

class autopilot_mode {
protected:
    std::optional<value> hdg_;
    std::optional<value> nav_;
    std::optional<value> apr_;
    std::optional<value> rev_;
    std::optional<value> alt_;
    std::optional<value> vs_;
    std::optional<value> ias_;
    value ap_;

    inline
    autopilot_mode(
        std::optional<value> && hdg,
        std::optional<value> && nav,
        std::optional<value> && apr,
        std::optional<value> && rev,
        std::optional<value> && alt,
        std::optional<value> && vs,
        std::optional<value> && ias,
        value && ap
    ) noexcept :
        hdg_(std::move(hdg)),
        nav_(std::move(nav)),
        apr_(std::move(apr)),
        rev_(std::move(rev)),
        alt_(std::move(alt)),
        vs_(std::move(vs)),
        ias_(std::move(ias)),
        ap_(std::move(ap))
    {}
public:

    static
    std::expected<autopilot_mode, int>
    build(const YAML::Node & node);

    inline
    const std::optional<value> &
    hdg() const { return this->hdg_; }

    inline
    const std::optional<value> &
    nav() const { return this->nav_; }

    inline
    const std::optional<value> &
    apr() const { return this->apr_; }

    inline
    const std::optional<value> &
    rev() const { return this->rev_; }

    inline
    const std::optional<value> &
    alt() const { return this->alt_; }

    inline
    const std::optional<value> &
    vs() const { return this->vs_; }

    inline
    const std::optional<value> &
    ias() const { return this->ias_; }

    inline
    const value &
    ap() const { return this->ap_; }
};

}

#endif