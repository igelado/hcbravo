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
std::expected<std::optional<T>, int>
build_optional(const YAML::Node & node, const std::string & key)
{
    logger() << "Checking for '" << key << "'";
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

    if(!node["ap"]) {
        logger() << "Autopilot mode configuration has not autopilot";
        return std::unexpected(0);
    }
    auto ap = value::build(node["ap"]);
    if(ap.has_value() == false) return std::unexpected(ap.error());

    return autopilot_mode(
        std::move(hdg.value()), std::move(nav.value()), std::move(apr.value()), 
        std::move(rev.value()), std::move(alt.value()), std::move(vs.value()),
        std::move(ias.value()), std::move(ap.value())
    );
}

}