// SPDX-License-Identifier: LGPL-2.1-only
//
// src/conf.cpp
// XPlane Plugin for HoneyComb Bravo Throttle Controller
//
// Copyright (C) 2005 Isaac Gelado

#include "conf.h"

namespace conf {

std::expected<conf::key, int>
key::build(const YAML::Node & node)
{
    bool invert = node["invert"] ? node["invert"].as<bool>() : false;
    std::optional<int> index;
    if(node["index"]) {
        int parsed_index = node["index"].as<int>();
        if(parsed_index < 0) {
            logger() << "Invalid negative index in DataRef node '" << node << "'";
            return std::unexpected(0);
        }
        index = parsed_index;
    }
    if(node.IsMap()) {
        return key(node["key"].as<std::string>(), invert, index);
    }
    else if(node.IsScalar()) {
        return key(node.as<std::string>(), invert, index);
    }
    else {
        logger() << "Invalid DataRef node '" << node << "'";
        return std::unexpected(1);
    }

}

std::expected<conf::value, int>
value::build(const YAML::Node & node)
{
    std::vector<key> keys;

    if(!node) return value(std::move(keys));

    if(node.IsSequence()) {
        for(const auto & value : node) {
            auto key = key::build(node);
            if(key.has_value()) keys.emplace_back(std::move(key.value()));
            else return std::unexpected(key.error());
        }
    }
    else {
        auto key = key::build(node);
        if(key.has_value()) keys.emplace_back(std::move(key.value()));
        else return std::unexpected(key.error());
    }
    return value(std::move(keys));
}

std::expected<airspeed, int>
airspeed::build(const YAML::Node & node)
{
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
    
    auto is_mach = key::build(node["is_mach"]);
    if(is_mach.has_value() == false) return std::unexpected(is_mach.error());
    auto val = value::build(node["value"]);
    if(val.has_value() == false) return std::unexpected(val.error());
    return airspeed(std::move(is_mach.value()), std::move(val.value()));
}

template<typename T>
static inline
std::expected<T, int>
build(const YAML::Node & node, const std::string & key)
{
    logger() << "Checking for required '" << key << "'";
    if(!node.IsMap() or !node[key]) {
        logger() << "Key '" << key << "' not found in: " << node;
        return std::unexpected(0);
    }
    auto ret = T::build(node[key]);
    if(ret.has_value() == false) {
        return std::unexpected(ret.error());
    }
    return std::move(ret.value());
}

template<typename T>
static inline
std::expected<T, int>
build(const YAML::Node & node, const std::string & key,
                const std::string & alias)
{
    auto ret = build<T>(node, key);
    if(ret.has_value()) return ret;
    return build<T>(node, alias);
}

template<typename T>
static inline
std::expected<std::optional<T>, int>
build_optional(const YAML::Node & node, const std::string & key)
{
    logger() << "Checking for optional '" << key << "'";
    if(!node.IsMap() or !node[key]) {
        logger() << "Key '" << key << "' not found in: " << node;
        return std::nullopt;
    }
    auto ret = T::build(node[key]);
    if(ret.has_value() == false) {
        return std::unexpected(ret.error());
    }
    return std::optional(std::move(ret.value()));
}

template<typename T>
static inline
std::expected<std::optional<T>, int>
build_optional(const YAML::Node & node, const std::string & key,
                const std::string & alias)
{
    auto ret = build_optional<T>(node, key);
    if(ret.has_value()) return ret;
    return build_optional<T>(node, alias);
}

std::expected<autopilot_dial, int>
autopilot_dial::build(const YAML::Node & node)
{
    auto ias = build_optional<airspeed>(node, "ias");
    if(ias.has_value() == false)
        return std::unexpected(ias.error());
    auto course = build_optional<value>(node, "crs", "course");
    if(course.has_value() == false)
        return std::unexpected(course.error());

    auto heading = build_optional<value>(node, "hdg", "heading");
    if(heading.has_value() == false)
        return std::unexpected(course.error());

    auto vs = build_optional<value>(node, "vs");
    if(vs.has_value() == false)
        return std::unexpected(vs.error());

    auto alt = build_optional<value>(node, "alt");
    if(alt.has_value() == false)
        return std::unexpected(alt.error());

    return autopilot_dial(
        std::move(ias.value()),
        std::move(course.value()),
        std::move(heading.value()),
        std::move(vs.value()),
        std::move(alt.value())
    );
}

std::expected<autopilot_mode, int>
autopilot_mode::build(const YAML::Node & node)
{
    auto hdg = build_optional<value>(node, "hdg");
    if(hdg.has_value() == false) {
        return std::unexpected(hdg.error());
    }

    auto nav = build_optional<value>(node, "nav");
    if(nav.has_value() == false) {
        return std::unexpected(nav.error());
    }

    auto apr = build_optional<value>(node, "apr");
    if(apr.has_value() == false) {
        return std::unexpected(apr.error());
    }

    auto rev = build_optional<value>(node, "rev");
    if(rev.has_value() == false) {
        return std::unexpected(rev.error());
    }

    auto alt = build_optional<value>(node, "alt");
    if(alt.has_value() == false) {
        return std::unexpected(alt.error());
    }

    auto vs = build_optional<value>(node, "vs");
    if(vs.has_value() == false) {
        return std::unexpected(vs.error());
    }

    auto ias = build_optional<value>(node, "ias");
    if(ias.has_value() == false) {
        return std::unexpected(ias.error());
    }

    auto ap = conf::build<value>(node, "ap");
    if(ap.has_value() == false) return std::unexpected(ap.error());

    return autopilot_mode(
        std::move(hdg.value()), std::move(nav.value()), std::move(apr.value()), 
        std::move(rev.value()), std::move(alt.value()), std::move(vs.value()),
        std::move(ias.value()), std::move(ap.value())
    );
}

std::expected<autopilot, int>
autopilot::build(const YAML::Node & node)
{
    if(!node.IsMap()) {
        logger() << "Autopilot node has invalid format";
        return std::unexpected(0);
    }
    if(!node["mode"]) {
        logger() << "Autopilot node missing mode";
        return std::unexpected(0);
    }
    auto mode = autopilot_mode::build(node["mode"]);
    if(mode.has_value() == false) return std::unexpected(mode.error());
    auto dials = build_optional<autopilot_dial>(node, "dials");
    if(dials.has_value() == false) return std::unexpected(dials.error());
    return autopilot(std::move(mode.value()), std::move(dials.value()));
}

std::expected<gear, int>
gear::build(const YAML::Node & node)
{
    if(!node.IsMap()) {
        logger() << "Gear node has invalid format";
        return std::unexpected(0);
    }
    auto left = conf::build<value>(node, "left");
    if(left.has_value() == false) return std::unexpected(left.error());
    auto nose = conf::build<value>(node, "nose");
    if(nose.has_value() == false) return std::unexpected(nose.error());
    auto right = conf::build<value>(node, "right");
    if(right.has_value() == false) return std::unexpected(right.error());

    return gear(std::move(left.value()), std::move(nose.value()), std::move(right.value()));
}

std::expected<system, int>
system::build(const YAML::Node & node)
{
    if(!node.IsMap()) {
        logger() << "System node has invalid format";
        return std::unexpected(0);
    }
    auto volts = conf::build<value>(node, "volts");
    if(volts.has_value() == false) return std::unexpected(volts.error());
    auto gear = build_optional<conf::gear>(node, "gear");
    if(gear.has_value() == false) return std::unexpected(gear.error());

    return system(std::move(volts.value()), std::move(gear.value()));
}

std::expected<annunciator, int>
annunciator::build(const YAML::Node & node)
{
    if(!node.IsMap()) {
        logger() << "Annunciator node has invalid format";
        return std::unexpected(0);
    }

    auto master_warn = build_optional<value>(node, "master_warn");
    if(master_warn.has_value() == false) return std::unexpected(master_warn.error());   
    auto eng_fire = build_optional<value>(node, "eng_fire");
    if(eng_fire.has_value() == false) return std::unexpected(eng_fire.error());
    auto oil_low = build_optional<value>(node, "oil_low"); 
    if(oil_low.has_value() == false) return std::unexpected(oil_low.error());
    auto fuel_low = build_optional<value>(node, "fuel_low");
    if(fuel_low.has_value() == false) return std::unexpected(fuel_low.error());
    auto anti_ice = build_optional<value>(node, "anti_ice");
    if(anti_ice.has_value() == false) return std::unexpected(anti_ice.error());
    auto starter = build_optional<value>(node, "starter");
    if(starter.has_value() == false) return std::unexpected(starter.error());
    auto apu = build_optional<value>(node, "apu");
    if(apu.has_value() == false) return std::unexpected(apu.error());
    auto master_caution = build_optional<value>(node, "master_caution");
    if(master_caution.has_value() == false) return std::unexpected(master_caution.error()); 
    auto vacuum_low = build_optional<value>(node, "vacuum_low");
    if(vacuum_low.has_value() == false) return std::unexpected(vacuum_low.error());
    auto hydro_low = build_optional<value>(node, "hydro_low");
    if(hydro_low.has_value() == false) return std::unexpected(hydro_low.error());
    auto aux_fuel = build_optional<value>(node, "aux_fuel");
    if(aux_fuel.has_value() == false) return std::unexpected(aux_fuel.error());
    auto parking_brake = build_optional<value>(node, "parking_brake");
    if(parking_brake.has_value() == false) return std::unexpected(parking_brake.error());
    auto volt_low = build_optional<value>(node, "volt_low");
    if(volt_low.has_value() == false) return std::unexpected(volt_low.error());
    auto door_open = build_optional<value>(node, "door_open");
    if(door_open.has_value() == false) return std::unexpected(door_open.error());
    return annunciator(
        std::move(master_warn.value()), std::move(eng_fire.value()),
        std::move(oil_low.value()), std::move(fuel_low.value()),
        std::move(anti_ice.value()), std::move(starter.value()),
        std::move(apu.value()), std::move(master_caution.value()),
        std::move(vacuum_low.value()), std::move(hydro_low.value()),
        std::move(aux_fuel.value()), std::move(parking_brake.value()),
        std::move(volt_low.value()), std::move(door_open.value())
    );
}

std::expected<configuration, int>
configuration::read(const std::string & path)
{
    try {
        logger() << "Loading YAML File " << path;
        YAML::Node node = YAML::LoadFile(path);

        if(!node["name"]) {
            logger() << "Configuration '" << path << "' missing name";
            return std::unexpected(0);
        }
    
        auto name = node["name"].as<std::string>();

        std::vector<std::string> aircrafts;
        if(!node["aircrafts"] or node["aircrafts"].IsSequence() == false) {
            logger() << "Configuration '" << path 
                     << "' does not include supported aircrafts";
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
            logger() << "Configuration does not include supported models";
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
            logger() << "No models defined for this configuration";
            return std::unexpected(0);
        }

        logger() << "Reading System Configuration";
        auto system = conf::build<conf::system>(node, "system");
        if(system.has_value() == false) return std::unexpected(system.error());

        logger() << "Reading Autopilot Configuration";
        auto autopilot = conf::build_optional<conf::autopilot>(node, "autopilot");
        if(autopilot.has_value() == false) return std::unexpected(autopilot.error());

        logger() << "Reading Annunciator Configuration";
        auto annunciator = conf::build_optional<conf::annunciator>(node, "annunciator");
        if(annunciator.has_value() == false) return std::unexpected(annunciator.error());   

        return configuration(
            std::move(name), std::move(aircrafts),
            std::move(models), std::move(system.value()),
            std::move(autopilot.value()), std::move(annunciator.value())
        );
    }
    catch(const YAML::Exception & ex) {
        logger() << "Failed to parse YAML file '" << path << "': " << ex.what();
    }
    catch(const std::exception & ex) {
        logger() << "Failed to load YAML file '" << path << "': " << ex.what();
    }
    return std::unexpected(0);
}
    
}