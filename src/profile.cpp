#include "logger.h"
#include "profile.h"

#include <XPLM/XPLMUtilities.h>

#include <memory>
#include <optional>

base_data_ref::base_data_ref(const YAML::Node & node) noexcept :
    data_ref_(nullptr),
    invert_(false),
    index_(std::nullopt)
{
    if(!node) return;
    if(node.IsMap()) {
        data_ref_ = XPLMFindDataRef(node["key"].as<std::string>().c_str());
        if(node["index"]) this->index_ = node["index"].as<size_t>();
        if(node["invert"]) this->invert_ = node["invert"].as<bool>();
    }
    else if(node.IsScalar()) {
        data_ref_ = XPLMFindDataRef(node.as<std::string>().c_str());
    }
    else {
        logger() << "Invalid node " << node;
        return;
    }

    // TODO: Detect array values and add a zero-index if no index
    // is specified
}

template<>
class data_ref<bool> : public bool_data_ref {
public:
    inline
    data_ref(const YAML::Node & node) noexcept : bool_data_ref(node) {}

    data_ref(data_ref && other) noexcept = default;

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
private:
    std::vector<int> values_;
public:
    inline
    data_ref(const YAML::Node & node) noexcept : bool_data_ref(node) {
        if(node.IsMap()) {
            for(const auto v : node["values"]) {
                values_.emplace_back(v.as<int>());
            }
        }
    }

    data_ref(data_ref && other) noexcept = default;

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


static
std::optional<bool_data_ref::ptr_type>
make_bool_data_ref(const YAML::Node & node) noexcept
{
    if(!node or !node["key"]) return std::nullopt;
    std::string node_type = node["type"] ? node["type"].as<std::string>() : "bool";

    if(node_type == "bool") {
        return bool_data_ref::ptr_type(new data_ref<bool>(node));
    }
    else if(node_type == "int") {
        return bool_data_ref::ptr_type(new data_ref<int>(node));
    }
    else if(node_type == "float") {
        return bool_data_ref::ptr_type(new data_ref<float>(node));
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
    XPLMDataRef && is_mach, XPLMDataRef && value
) noexcept :
    is_mach_(std::move(is_mach)),
    value_(std::move(value))
{}

std::expected<airspeed_data_ref, int>
airspeed_data_ref::build(const YAML::Node & node) noexcept {
    if(!node["is_mach"] or !node["value"]) return std::unexpected(0);
    
    auto is_mach = XPLMFindDataRef(node["is_mach"].as<std::string>().c_str());
    auto value = XPLMFindDataRef(node["value"].as<std::string>().c_str());
    if(is_mach == nullptr or value == nullptr) return std::unexpected(0);

    return airspeed_data_ref(std::move(is_mach), std::move(value)); 
}

autopilot_dial_data_ref::autopilot_dial_data_ref(std::optional<airspeed_data_ref> && ias, const YAML::Node & node) noexcept :
    ias_(std::move(ias)),
    course_(node["course"] ? std::optional(float_data_ref(node["course"])) : std::nullopt),
    heading_(node["heading"] ? std::optional(float_data_ref(node["heading"])) : std::nullopt),
    vs_(node["vs"] ? std::optional(float_data_ref(node["vs"])) : std::nullopt),
    alt_(node["alt"] ? std::optional(float_data_ref(node["alt"])) : std::nullopt)
{}

std::expected<autopilot_dial_data_ref, int>
autopilot_dial_data_ref::build(const YAML::Node & node) noexcept
{
    logger() << "Reading Autopilot Dials";
    if(node["airspeed"]) {
        auto ias = airspeed_data_ref::build(node["airspeed"]);
        if(ias.has_value() == false) return std::unexpected(0);
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
    if(mode.has_value() == false) return std::unexpected(0);

    if(node["dials"]) {
        auto dial = autopilot_dial_data_ref::build(node["dials"]);
        if(dial.has_value() == false) return std::unexpected(0);

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


profile::profile(std::string && name, std::vector<std::string> && models, 
    system_data_ref && system, std::optional<autopilot_data_ref> && autopilot,
    std::optional<annunciator_data_ref> && annunciator
) noexcept :
    name_(std::move(name)),
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
    if(!node["models"] or node["models"].IsSequence() == false) {
        logger() << "Profile does not include supported models";
        return std::unexpected(0);
    }
    std::vector<std::string> models;
    for(const auto & model : node["models"]) {
        if(model.Type() != YAML::NodeType::Scalar) {
            logger() << "Model value '" << node << "' is not valid";
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
        std::move(models),
        std::move(system.value()),
        std::move(autopilot),
        std::move(annunciator)
    ));
}