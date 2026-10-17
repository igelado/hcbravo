// SPDX-License-Identifier: LGPL-2.1-only
//
// src/conf.cpp
// XPlane Plugin for HoneyComb Bravo Throttle Controller
//
// Copyright (C) 2005 Isaac Gelado

#include "conf.h"

#include <initializer_list>
#include <string>
#include <string_view>
#include <unordered_set>

namespace conf {

std::expected<void, std::string> validate_file(const std::string & path) {
    const auto result = configuration::read(path);
    if(!result) return std::unexpected("invalid configuration (see parser diagnostics)");
    return {};
}

static
std::string
yaml_location(const YAML::Mark & mark)
{
    if(mark.is_null()) return {};

    return " at line " + std::to_string(mark.line + 1)
         + ", column " + std::to_string(mark.column + 1);
}

static
std::string
yaml_location(const YAML::Node & node)
{
    return yaml_location(node.Mark());
}

static
std::expected<void, int>
validate_map_keys(const YAML::Node & node, const std::string & description,
                  std::initializer_list<std::string_view> allowed_keys)
{
    if(!node.IsMap()) {
        logger() << description << " must be a map" << yaml_location(node)
                 << ": '" << node << "'";
        return std::unexpected(0);
    }

    try {
        std::unordered_set<std::string> seen;
        for(const auto & entry : node) {
            if(!entry.first.IsScalar()) {
                logger() << description << " has a non-scalar key"
                         << yaml_location(entry.first) << ": '" << node << "'";
                return std::unexpected(0);
            }

            const auto key = entry.first.as<std::string>();
            if(!seen.insert(key).second) {
                logger() << "Duplicate " << description << " key '" << key
                         << "'" << yaml_location(entry.first);
                return std::unexpected(0);
            }
            bool allowed = false;
            for(const auto allowed_key : allowed_keys) {
                if(key == allowed_key) {
                    allowed = true;
                    break;
                }
            }
            if(!allowed) {
                logger() << "Unexpected " << description << " key '" << key
                         << "'" << yaml_location(entry.first) << " in node '" << node << "'";
                return std::unexpected(0);
            }
        }
    }
    catch(const YAML::Exception & ex) {
        logger() << "Invalid " << description << " node" << yaml_location(node) << " '" << node
                 << "': " << ex.what();
        return std::unexpected(0);
    }

    return {};
}

std::expected<conf::key, int>
key::build(const YAML::Node & node)
{
    if(node.IsScalar()) {
        try {
            auto data_ref = node.as<std::string>();
            if(data_ref.empty()) {
                logger() << "DataRef key cannot be empty" << yaml_location(node);
                return std::unexpected(0);
            }
            return key(std::move(data_ref), false, std::nullopt, {});
        }
        catch(const YAML::Exception & ex) {
            logger() << "Invalid scalar DataRef node" << yaml_location(node)
                     << " '" << node << "': " << ex.what();
            return std::unexpected(0);
        }
    }

    if(!validate_map_keys(node, "DataRef", {"key", "invert", "index", "values"})) {
        return std::unexpected(1);
    }

    const auto key_node = node["key"];
    if(!key_node || !key_node.IsScalar()) {
        logger() << "DataRef map is missing a scalar 'key'" << yaml_location(node)
                 << ": '" << node << "'";
        return std::unexpected(0);
    }

    try {
        auto data_ref = key_node.as<std::string>();
        if(data_ref.empty()) {
            logger() << "DataRef key cannot be empty" << yaml_location(key_node);
            return std::unexpected(0);
        }

        const bool invert = node["invert"] ? node["invert"].as<bool>() : false;
        std::optional<int> index;
        if(node["index"]) {
            const int parsed_index = node["index"].as<int>();
            if(parsed_index < 0) {
                logger() << "Invalid negative index" << yaml_location(node["index"])
                         << " in DataRef node '" << node << "'";
                return std::unexpected(0);
            }
            index = parsed_index;
        }

        std::vector<std::string> values;
        if(node["values"]) {
            const auto values_node = node["values"];
            if(!values_node.IsSequence() || values_node.size() == 0) {
                logger() << "DataRef values must be a non-empty sequence"
                         << yaml_location(values_node) << " in node '" << node << "'";
                return std::unexpected(0);
            }
            for(const auto & value : values_node) {
                if(!value.IsScalar()) {
                    logger() << "DataRef values must contain only scalars"
                             << yaml_location(value) << " in node '" << node << "'";
                    return std::unexpected(0);
                }
                values.emplace_back(value.as<std::string>());
            }
        }
        return key(std::move(data_ref), invert, index, std::move(values));
    }
    catch(const YAML::Exception & ex) {
        logger() << "Invalid DataRef map" << yaml_location(node)
                 << " '" << node << "': " << ex.what();
        return std::unexpected(0);
    }
}

std::expected<conf::value, int>
value::build(const YAML::Node & node)
{
    std::vector<key> keys;

    if(!node) return value(std::move(keys));

    if(node.IsSequence()) {
        if(node.size() == 0) {
            logger() << "DataRef value cannot be an empty sequence" << yaml_location(node);
            return std::unexpected(0);
        }
        for(const auto & value : node) {
            auto key = key::build(value);
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

static std::expected<value, int>
build_dial_value(const YAML::Node & node)
{
    auto result = value::build(node);
    if(!result) return std::unexpected(result.error());
    if(result->keys().size() != 1) {
        logger() << "Autopilot dial must contain exactly one DataRef" << yaml_location(node);
        return std::unexpected(0);
    }
    return result;
}

static std::expected<std::optional<value>, int>
build_optional_dial(const YAML::Node & node, const char * key)
{
    if(!node[key]) return std::optional<value> {};
    auto result = build_dial_value(node[key]);
    if(!result) return std::unexpected(result.error());
    return std::optional<value>(std::move(result.value()));
}

std::expected<airspeed, int>
airspeed::build(const YAML::Node & node)
{
    if(!validate_map_keys(node, "IAS", {"is_mach", "value"})) {
        return std::unexpected(0);
    }
    else if(!node["is_mach"]) {
        logger() << "IAS missing Mach node" << yaml_location(node);
        return std::unexpected(0);
    }
    else if(!node["value"]) {
        logger() << "IAS missing Value node" << yaml_location(node);
        return std::unexpected(0);
    }
    
    auto is_mach = key::build(node["is_mach"]);
    if(is_mach.has_value() == false) return std::unexpected(is_mach.error());
    auto val = build_dial_value(node["value"]);
    if(val.has_value() == false) return std::unexpected(val.error());
    return airspeed(std::move(is_mach.value()), std::move(val.value()));
}

template<typename T>
static inline
std::expected<T, int>
build(const YAML::Node & node, const std::string & key)
{
#if !defined(NDEBUG)
    if(diagnostics::enabled(diagnostic_category::configuration))
        debug_logger() << "[Configuration debug] Checking for required '" << key << "'";
#endif
    if(!node.IsMap() or !node[key]) {
        logger() << "Key '" << key << "' not found" << yaml_location(node) << ": " << node;
        return std::unexpected(0);
    }
    auto ret = T::build(node[key]);
    if(ret.has_value() == false) {
        return std::unexpected(ret.error());
    }
    return std::move(ret.value());
}

static inline
std::expected<std::vector<std::string>, int>
build_required_string_list(const YAML::Node & node, const std::string & key)
{
    const auto values = node[key];
    if(!values || !values.IsSequence() || values.size() == 0) {
        logger() << "Configuration requires a non-empty '" << key << "' sequence"
                 << yaml_location(node);
        return std::unexpected(0);
    }

    std::vector<std::string> result;
    for(const auto & value : values) {
        if(!value.IsScalar()) {
            logger() << "Configuration '" << key << "' entries must be scalars"
                     << yaml_location(value);
            return std::unexpected(0);
        }
        auto string_value = value.as<std::string>();
        if(string_value.empty()) {
            logger() << "Configuration '" << key << "' entries cannot be empty"
                     << yaml_location(value);
            return std::unexpected(0);
        }
        result.emplace_back(std::move(string_value));
    }
    return result;
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
    if(!node.IsMap() or !node[key]) {
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
    if(node.IsMap() && node[key]) {
        return build_optional<T>(node, key);
    }
    return build_optional<T>(node, alias);
}

std::expected<autopilot_dial, int>
autopilot_dial::build(const YAML::Node & node)
{
    if(!validate_map_keys(node, "Autopilot dials",
                          {"ias", "crs", "course", "hdg", "heading", "vs", "alt"})) {
        return std::unexpected(0);
    }
    if(node["crs"] && node["course"]) {
        logger() << "Autopilot dials cannot define both 'crs' and 'course'"
                 << yaml_location(node);
        return std::unexpected(0);
    }
    if(node["hdg"] && node["heading"]) {
        logger() << "Autopilot dials cannot define both 'hdg' and 'heading'"
                 << yaml_location(node);
        return std::unexpected(0);
    }

    auto ias = build_optional<airspeed>(node, "ias");
    if(ias.has_value() == false)
        return std::unexpected(ias.error());
    auto course = build_optional_dial(node, node["crs"] ? "crs" : "course");
    if(course.has_value() == false)
        return std::unexpected(course.error());

    auto heading = build_optional_dial(node, node["hdg"] ? "hdg" : "heading");
    if(heading.has_value() == false)
        return std::unexpected(heading.error());

    auto vs = build_optional_dial(node, "vs");
    if(vs.has_value() == false)
        return std::unexpected(vs.error());

    auto alt = build_optional_dial(node, "alt");
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
    if(!validate_map_keys(node, "Autopilot modes",
                          {"hdg", "nav", "apr", "rev", "alt", "vs", "ias", "ap"})) {
        return std::unexpected(0);
    }

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
    if(!validate_map_keys(node, "Autopilot", {"dials", "modes"})) {
        return std::unexpected(0);
    }
    if(!node["modes"]) {
        logger() << "Autopilot node missing modes" << yaml_location(node);
        return std::unexpected(0);
    }
    auto mode = autopilot_mode::build(node["modes"]);
    if(mode.has_value() == false) return std::unexpected(mode.error());
    auto dials = build_optional<autopilot_dial>(node, "dials");
    if(dials.has_value() == false) return std::unexpected(dials.error());
    return autopilot(std::move(mode.value()), std::move(dials.value()));
}

std::expected<gear_signals, int>
gear_signals::build(const YAML::Node & node)
{
    if(!validate_map_keys(node, "Gear signals", {"safe", "unsafe"})) {
        return std::unexpected(0);
    }
    auto safe = conf::build<value>(node, "safe");
    if(safe.has_value() == false) return std::unexpected(safe.error());
    auto unsafe = conf::build<value>(node, "unsafe");
    if(unsafe.has_value() == false) return std::unexpected(unsafe.error());

    return gear_signals(std::move(safe.value()), std::move(unsafe.value()));
}

std::expected<gear, int>
gear::build(const YAML::Node & node)
{
    if(!validate_map_keys(node, "Gear", {"left", "nose", "right"})) {
        return std::unexpected(0);
    }
    auto left = conf::build<gear_signals>(node, "left");
    if(left.has_value() == false) return std::unexpected(left.error());
    auto nose = conf::build<gear_signals>(node, "nose");
    if(nose.has_value() == false) return std::unexpected(nose.error());
    auto right = conf::build<gear_signals>(node, "right");
    if(right.has_value() == false) return std::unexpected(right.error());

    return gear(std::move(left.value()), std::move(nose.value()), std::move(right.value()));
}

std::expected<system, int>
system::build(const YAML::Node & node)
{
    if(!validate_map_keys(node, "System", {"volts", "gear"})) {
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
    if(!validate_map_keys(node, "Annunciator", {
        "master_warn", "eng_fire", "oil_low", "fuel_low", "anti_ice", "starter",
        "apu", "master_caution", "vacuum_low", "hydro_low", "aux_fuel",
        "parking_brake", "volt_low", "door_open"
    })) {
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
#if !defined(NDEBUG)
        if(diagnostics::enabled(diagnostic_category::configuration))
            debug_logger() << "[Configuration debug] Loading YAML File " << path;
#endif
        YAML::Node node = YAML::LoadFile(path);

        if(!validate_map_keys(node, "Configuration '" + path + "'", {
            "name", "aircrafts", "models", "system", "autopilot", "annunciator"
        })) {
            return std::unexpected(0);
        }

        const auto name_node = node["name"];
        if(!name_node || !name_node.IsScalar()) {
            logger() << "Configuration '" << path << "' requires a scalar name"
                     << yaml_location(node);
            return std::unexpected(0);
        }
        auto name = name_node.as<std::string>();
        if(name.empty()) {
            logger() << "Configuration '" << path << "' name cannot be empty"
                     << yaml_location(name_node);
            return std::unexpected(0);
        }

        auto aircrafts = build_required_string_list(node, "aircrafts");
        if(!aircrafts.has_value()) return std::unexpected(aircrafts.error());
        auto models = build_required_string_list(node, "models");
        if(!models.has_value()) return std::unexpected(models.error());

#if !defined(NDEBUG)
        if(diagnostics::enabled(diagnostic_category::configuration))
            debug_logger() << "[Configuration debug] Reading System Configuration";
#endif
        auto system = conf::build<conf::system>(node, "system");
        if(system.has_value() == false) return std::unexpected(system.error());

#if !defined(NDEBUG)
        if(diagnostics::enabled(diagnostic_category::configuration))
            debug_logger() << "[Configuration debug] Reading Autopilot Configuration";
#endif
        auto autopilot = conf::build_optional<conf::autopilot>(node, "autopilot");
        if(autopilot.has_value() == false) return std::unexpected(autopilot.error());

#if !defined(NDEBUG)
        if(diagnostics::enabled(diagnostic_category::configuration))
            debug_logger() << "[Configuration debug] Reading Annunciator Configuration";
#endif
        auto annunciator = conf::build_optional<conf::annunciator>(node, "annunciator");
        if(annunciator.has_value() == false) return std::unexpected(annunciator.error());   

        return configuration(
            std::move(name), std::move(aircrafts.value()),
            std::move(models.value()), std::move(system.value()),
            std::move(autopilot.value()), std::move(annunciator.value())
        );
    }
    catch(const YAML::Exception & ex) {
        logger() << "Failed to parse YAML file '" << path << "'"
                 << yaml_location(ex.mark) << ": " << ex.what();
    }
    catch(const std::exception & ex) {
        logger() << "Failed to load YAML file '" << path << "': " << ex.what();
    }
    return std::unexpected(0);
}
    
}
