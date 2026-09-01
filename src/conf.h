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

class autopilot {
protected:
    autopilot_mode mode_;
    std::optional<autopilot_dial> dials_;

    inline
    autopilot(autopilot_mode && mode, std::optional<autopilot_dial> && dials) noexcept :
        mode_(std::move(mode)),
        dials_(std::move(dials))
    {}
public:

    static
    std::expected<autopilot, int>
    build(const YAML::Node & node);

    inline
    const autopilot_mode &
    mode() const noexcept { return this->mode_; }

    inline
    const std::optional<autopilot_dial> &
    dials() const noexcept { return this->dials_; }
};

class gear {
protected:
    value left_;
    value nose_;
    value right_;

    inline
    gear(value && left, value && nose, value && right) :
        left_(std::move(left)),
        nose_(std::move(nose)),
        right_(std::move(right))
    {}
public:

    static
    std::expected<gear, int>
    build(const YAML::Node & node);

    inline
    const value &
    left() const { return this->left_; }

    inline
    const value &
    nose() const { return this->nose_; }

    inline
    const value &
    right() const { return this->right_; }
};

class system {
protected:
    value volts_;
    std::optional<gear> gear_;

    inline
    system(value && volts, std::optional<gear> && gear) :
        volts_(std::move(volts)),
        gear_(std::move(gear))
    {}
public:

    static
    std::expected<system, int>
    build(const YAML::Node & node);

    inline
    const value &
    volts() const { return this->volts_; }

    inline
    const std::optional<gear> &
    gear() const { return this->gear_; }
};

class annunciator {
protected:
    std::optional<value> master_warn_;
    std::optional<value> eng_fire_;
    std::optional<value> oil_low_;
    std::optional<value> fuel_low_;
    std::optional<value> anti_ice_;
    std::optional<value> starter_;
    std::optional<value> apu_;
    std::optional<value> master_caution_;
    std::optional<value> vacuum_low_;
    std::optional<value> hydro_low_;
    std::optional<value> aux_fuel_;
    std::optional<value> parking_brake_;
    std::optional<value> volt_low_;
    std::optional<value> door_open_;

    inline
    annunciator(
        std::optional<value> && master_warn,
        std::optional<value> && eng_fire,
        std::optional<value> && oil_low,
        std::optional<value> && fuel_low,
        std::optional<value> && anti_ice,
        std::optional<value> && starter,
        std::optional<value> && apu,
        std::optional<value> && master_caution,
        std::optional<value> && vacuum_low,
        std::optional<value> && hydro_low,
        std::optional<value> && aux_fuel,
        std::optional<value> && parking_brake,
        std::optional<value> && volt_low,
        std::optional<value> && door_open
    ) noexcept :
        master_warn_(std::move(master_warn)),
        eng_fire_(std::move(eng_fire)),
        oil_low_(std::move(oil_low)),
        fuel_low_(std::move(fuel_low)),
        anti_ice_(std::move(anti_ice)),
        starter_(std::move(starter)),
        apu_(std::move(apu)),
        master_caution_(std::move(master_caution)),
        vacuum_low_(std::move(vacuum_low)),
        hydro_low_(std::move(hydro_low)),
        aux_fuel_(std::move(aux_fuel)),
        parking_brake_(std::move(parking_brake)),
        volt_low_(std::move(volt_low)),
        door_open_(std::move(door_open))
    {}
public:

    static
    std::expected<annunciator, int>
    build(const YAML::Node & node);

    inline
    const std::optional<value> &
    master_warn() const { return this->master_warn_; }

    inline
    const std::optional<value> &
    eng_fire() const { return this->eng_fire_; }

    inline
    const std::optional<value> &
    oil_low() const { return this->oil_low_; }

    inline
    const std::optional<value> &
    fuel_low() const { return this->fuel_low_; }

    inline
    const std::optional<value> &
    anti_ice() const { return this->anti_ice_; }

    inline
    const std::optional<value> &
    starter() const { return this->starter_; }

    inline
    const std::optional<value> &
    apu() const { return this->apu_; }

    inline
    const std::optional<value> &
    master_caution() const { return this->master_caution_; }

    inline
    const std::optional<value> &
    vacuum_low() const { return this->vacuum_low_; }

    inline
    const std::optional<value> &
    hydro_low() const { return this->hydro_low_; }

    inline
    const std::optional<value> &
    aux_fuel() const { return this->aux_fuel_; }

    inline
    const std::optional<value> &
    parking_brake() const { return this->parking_brake_; }

    inline
    const std::optional<value> &
    volt_low() const { return this->volt_low_; }

    inline
    const std::optional<value> &
    door_open() const { return this->door_open_; }
};

class configuration {
protected:
    std::string name_;
    std::vector<std::string> aircrafts_;
    std::vector<std::string> models_;
    system system_;
    std::optional<autopilot> autopilot_;
    std::optional<annunciator> annunciator_;

    inline
    configuration(
        std::string && name,
        std::vector<std::string> && aircrafts,
        std::vector<std::string> && models,
        system && system,
        std::optional<autopilot> && autopilot,
        std::optional<annunciator> && annunciator
    ) noexcept :
        name_(std::move(name)),
        aircrafts_(std::move(aircrafts)),
        models_(std::move(models)),
        system_(std::move(system)),
        autopilot_(std::move(autopilot)),
        annunciator_(std::move(annunciator))
    {}
public:

    static
    std::expected<configuration, int>
    read(const std::string & path);

    inline
    const std::string &
    name() const noexcept { return this->name_; }

    inline
    const std::vector<std::string> &
    aircrafts() const noexcept { return this->aircrafts_; }

    inline
    const std::vector<std::string> &
    models() const noexcept { return this->models_; }

    inline
    const system &
    system() const noexcept { return this->system_; }

    inline
    const std::optional<autopilot> &
    autopilot() const noexcept { return this->autopilot_; }

    inline
    const std::optional<annunciator> &
    annunciator() const noexcept { return this->annunciator_; }
};

}

#endif