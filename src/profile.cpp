// SPDX-License-Identifier: LGPL-2.1-only
//
// src/profile.cpp
// XPlane Plugin for HoneyComb Bravo Throttle Controller
//
// Copyright (C) 2005 Isaac Gelado

#include "logger.h"
#include "profile.h"

#include <XPLM/XPLMUtilities.h>

#include <memory>
#include <optional>


static
std::optional<bool_data_ref::ptr_type>
make_bool_data_ref(const YAML::Node & node) noexcept
{
    if(!node or !node["key"]) return std::nullopt;
    std::string node_type = node["type"] ? node["type"].as<std::string>() : "bool";

    if(node_type == "bool") {
        auto data = data_ref<bool>::build(node);
        if(data.has_value() == false) return std::nullopt;
        return bool_data_ref::ptr_type(new data_ref<bool>(std::move(data.value())));
    }
    else if(node_type == "int") {
        auto data = data_ref<int>::build(node);
        if(data.has_value() == false) return std::nullopt;
        return bool_data_ref::ptr_type(new data_ref<int>(std::move(data.value())));
    }
    else if(node_type == "float") {
        auto data = data_ref<float>::build(node);
        if(data.has_value() == false) return std::nullopt;
        return bool_data_ref::ptr_type(new data_ref<float>(std::move(data.value())));
    }
    return std::nullopt;
}

value_data_ref::value_data_ref(const YAML::Node & node) noexcept {
    if(!node or node.IsSequence() == false) return;
    for(const auto & value : node) {
        auto data = make_bool_data_ref(value);
        if(data.has_value()) data_.emplace_back(std::move(data.value()));
    }
}

airspeed_data_ref::airspeed_data_ref(
    XPLMDataRef && is_mach, data_ref<float> && value
) noexcept :
    is_mach_(std::move(is_mach)),
    value_(std::move(value))
{}

std::expected<airspeed_data_ref, int>
airspeed_data_ref::build(const YAML::Node & node) noexcept {
    if(!node.IsMap()) {
        logger() << "IAS node has invalid format";
        return std::unexpected(0);
    }
    else if(!node["is_mach"]) {
        logger() << "IAS missing Mach node";
        return std::unexpected(0);
    }
    else if(!node["value"]) {
        logger() << "IAS missing Value npde";
        return std::unexpected(0);
    }
    
    auto is_mach = XPLMFindDataRef(node["is_mach"].as<std::string>().c_str());
    if(is_mach == nullptr) {
        logger() << "Invalid IAS Mach node";
        return std::unexpected(0);
    }

    auto value = data_ref<float>::build(node["value"]);
    if(!value.has_value()) {
        logger() << "Invalid IAS Value node";
        return std::unexpected(0);
    }

    return airspeed_data_ref(std::move(is_mach), std::move(value.value())); 
}

template<typename T>
static inline
std::optional<data_ref<T>>
build_optional_data_ref(const YAML::Node & node, const std::string & key) noexcept
{
    logger() << "Checking for '" << key << "'";
    if(!node.IsMap() or !node[key]) {
        logger() << "Key '" << key << "' not found in: " << node;
        return std::nullopt;
    }
    auto ret = data_ref<T>::build(node[key]);
    if(ret.has_value() == false) {
        logger() << "DataRef in '" << node << "' not found";
        return std::nullopt;
    }
    return std::optional(std::move(ret.value()));
}

autopilot_dial_data_ref::autopilot_dial_data_ref(std::optional<airspeed_data_ref> && ias, const YAML::Node & node) noexcept :
    ias_(std::move(ias)),
    course_(build_optional_data_ref<float>(node, "crs")),
    heading_(build_optional_data_ref<float>(node, "hdg")),
    vs_(build_optional_data_ref<float>(node, "vs")),
    alt_(build_optional_data_ref<float>(node, "alt"))
{}

std::expected<autopilot_dial_data_ref, int>
autopilot_dial_data_ref::build(const YAML::Node & node) noexcept
{
    logger() << "Reading Autopilot Dials";
    if(node.IsMap() == false) {
        logger() << "Invalid Autopilot Configuration";
        return std::unexpected(0);
    }

    logger() << "Checking if IAS Dial is defined";
    if(node["ias"]) {
        auto ias = airspeed_data_ref::build(node["ias"]);
        if(ias.has_value() == false) {
            logger() << "Invalid IAS Dial Configuration";
            return std::unexpected(0);
        }
        return autopilot_dial_data_ref(std::move(ias.value()), node);
    }
    return autopilot_dial_data_ref(std::nullopt, node);
}

autopilot_mode_data_ref::autopilot_mode_data_ref(const YAML::Node & node) noexcept :
    hdg_(node["hdg"] ? std::optional(value_data_ref(node["hdg"])) : std::nullopt),
    nav_(node["nav"] ? std::optional(value_data_ref(node["nav"])) : std::nullopt),
    apr_(node["apr"] ? std::optional(value_data_ref(node["apr"])) : std::nullopt),
    rev_(node["rev"] ? std::optional(value_data_ref(node["rev"])) : std::nullopt),
    alt_(node["alt"] ? std::optional(value_data_ref(node["alt"])) : std::nullopt),
    vs_ (node["vs"] ? std::optional(value_data_ref(node["vs"])) : std::nullopt),
    ias_(node["ias"] ? std::optional(value_data_ref(node["ias"])) : std::nullopt),
    ap_(node["ap"])
{}

std::expected<autopilot_mode_data_ref, int>
autopilot_mode_data_ref::build(const YAML::Node & node) noexcept
{
    logger() << "Reading Autopilot Modes";
    // Only the AP annunciator is required
    if(!node["ap"]) return std::unexpected(1);
    
    return autopilot_mode_data_ref(node);
}


autopilot_data_ref::autopilot_data_ref(
    autopilot_mode_data_ref && mode,
    std::optional<autopilot_dial_data_ref> && dials
) noexcept :
    mode_(std::move(mode)),
    dials_(std::move(dials))
{}

std::expected<autopilot_data_ref, int>
autopilot_data_ref::build(const YAML::Node & node) noexcept
{
    if(!node["modes"]) {
        logger() << "No modes defined for Autopilot";
        return std::unexpected(0);
    }
    auto mode = autopilot_mode_data_ref::build(node["modes"]);
    if(mode.has_value() == false) {
        logger() << "Invalid Autopilot Modes Configuration";
        return std::unexpected(0);
    }

    if(node["dials"]) {
        auto dial = autopilot_dial_data_ref::build(node["dials"]);
        if(dial.has_value() == false) {
            logger() << "Invalid Autopilot Dials Configuration";
            return std::unexpected(0);
        }

        return autopilot_data_ref(std::move(mode.value()), std::move(dial.value()));
    }

    return autopilot_data_ref(std::move(mode.value()), std::nullopt);
}

system_data_ref::system_data_ref(const YAML::Node & node) noexcept :
    volts_(node["volts"]),
    gear_(node["gear"] ? std::optional(value_data_ref(node["gear"])) : std::nullopt)
{}

std::expected<system_data_ref, int>
system_data_ref::build(const YAML::Node & node) noexcept
{
    // Only the AP annunciator is required
    if(!node["volts"]) return std::unexpected(1);
    return system_data_ref(node);
}



annunciator_data_ref::annunciator_data_ref(const YAML::Node & node) noexcept :
    master_warn_(node["master_warn"] ? std::optional(value_data_ref(node["master_warn"])) : std::nullopt),
    eng_fire_(node["eng_fire"] ? std::optional(value_data_ref(node["eng_fire"])) : std::nullopt),
    oil_low_(node["oil_low"] ? std::optional(value_data_ref(node["oil_low"])) : std::nullopt),
    fuel_low_(node["fuel_low"] ? std::optional(value_data_ref(node["fuel_low"])) : std::nullopt),
    anti_ice_(node["anti_ice"] ? std::optional(value_data_ref(node["anti_ice"])) : std::nullopt),
    starter_(node["starter"] ? std::optional(value_data_ref(node["starter"])) : std::nullopt),
    apu_(node["apu"] ? std::optional(value_data_ref(node["apu"])) : std::nullopt),
    master_caution_(node["master_caution"] ? std::optional(value_data_ref(node["master_caution"])) : std::nullopt),
    vacuum_low_(node["vacuum_low"] ? std::optional(value_data_ref(node["vacuum_low"])) : std::nullopt),
    hydro_low_(node["hydro_low"] ? std::optional(value_data_ref(node["hydro_low"])) : std::nullopt),
    aux_fuel_(node["aux_fuel"] ? std::optional(value_data_ref(node["aux_fuel"])) : std::nullopt),
    parking_brake_(node["parking_brake"] ? std::optional(value_data_ref(node["parking_brake"])) : std::nullopt),
    volt_low_(node["volt_low"] ? std::optional(value_data_ref(node["volt_low"])) : std::nullopt),
    door_open_(node["door_open"] ? std::optional(value_data_ref(node["door_open"])) : std::nullopt)
{}

std::expected<annunciator_data_ref, int>
annunciator_data_ref::build(const YAML::Node & node) noexcept 
{
    logger() << "Reading Annunciator";
    return annunciator_data_ref(node);
}


profile::profile(std::string && name,
    std::vector<std::string> && aircrafts, std::vector<std::string> && models, 
    system_data_ref && system, std::optional<autopilot_data_ref> && autopilot,
    std::optional<annunciator_data_ref> && annunciator
) noexcept :
    name_(std::move(name)),
    aircrafts_(std::move(aircrafts)),
    models_(std::move(models)),
    system_(std::move(system)),
    autopilot_(std::move(autopilot)),
    annunciator_(std::move(annunciator))
{}

std::expected<profile::ptr_type, int>
profile::from_yaml(const std::string & path) noexcept {
    logger() << "Loading YAML File " << path;
    YAML::Node node = YAML::LoadFile(path);
    if(!node["name"]) {
        logger() << "Profile does not include a name";
        return std::unexpected(0);
    }
    
    std::vector<std::string> aircrafts;
    if(!node["aircrafts"] or node["aircrafts"].IsSequence() == false) {
        logger() << "Profile does not include supported aircrafts";
    }
    else {
        for(const auto & aircraft : node["aircrafts"]) {
            if(aircraft.Type() != YAML::NodeType::Scalar) {
                logger() << "Invalid Aircraft '" << node << "'";
                continue;
            }
            aircrafts.emplace_back(aircraft.as<std::string>());
        }
    }

    if(!node["models"] or node["models"].IsSequence() == false) {
        logger() << "Profile does not include supported models";
        return std::unexpected(0);
    }
    std::vector<std::string> models;
    for(const auto & model : node["models"]) {
        if(model.Type() != YAML::NodeType::Scalar) {
            logger() << "Invalid Model '" << node << "'";
            continue;
        }
        models.emplace_back(model.as<std::string>());
    }
    if(models.empty()) {
        logger() << "No models defined for this profile";
        return std::unexpected(0);
    }

    logger() << "Reading System Configuration";
    if(!node["system"]) return std::unexpected(0);
    auto system = system_data_ref::build(node["system"]);
    if(system.has_value() == false) return std::unexpected(0);

    logger() << "Reading Autopilot Configuration";
    std::optional<autopilot_data_ref> autopilot;
    if(node["autopilot"]) {
        auto ap_ret = autopilot_data_ref::build(node["autopilot"]);
        if(ap_ret.has_value() == false) {
            logger() << "Invalid Autopilot Configuration";
            return std::unexpected(0);
        }
        autopilot = std::move(ap_ret.value());
    }

    logger() << "Reading Annunciator Configuration";
    std::optional<annunciator_data_ref> annunciator;
    if(node["annunciator"]) {
        auto ann_ret = annunciator_data_ref::build(node["annunciator"]);
        if(ann_ret.has_value() == false) {
            logger() << "Invalid Annunciator Configuration";
            return std::unexpected(0);
        }
        annunciator = std::move(ann_ret.value());
    }

    return profile_ptr(new profile(
        std::move(node["name"].as<std::string>()),
        std::move(aircrafts),
        std::move(models),
        std::move(system.value()),
        std::move(autopilot),
        std::move(annunciator)
    ));
}