#include "profile.h"

#include <memory>
#include <optional>

base_data_ref::base_data_ref(const YAML::Node & node) noexcept :
    data_ref_(nullptr)
{
    if(!node) return;
    data_ref_ = XPLMFindDataRef(node["key"].as<std::string>().c_str());
}

base_data_ref::~base_data_ref() noexcept {}

template<>
class data_ref<bool> : public base_data_ref {
public:
    inline
    data_ref(const YAML::Node & node) noexcept : base_data_ref(node) {}


    bool is_set() const final {
        if(this->data_ref_ == nullptr) return false;
        return XPLMGetDatai(this->data_ref_) != 0;
    }
};

template<>
class data_ref<int> : public base_data_ref {
private:
    std::vector<int> values_;
public:
    inline
    data_ref(const YAML::Node & node) noexcept : base_data_ref(node) {
        for(const auto v : node["values"]) {
            values_.emplace_back(v.as<int>());
        }
    }

    bool is_set() const final {
        if(this->data_ref_ == nullptr) return false;
        int value = XPLMGetDatai(this->data_ref_);
        if(value == 0) return false;
        for(const auto & v : this->values_) {
            if(v == value) return true;
        }
        return false;
    }
};


static
std::optional<base_data_ref::ptr_type>
make_data_ref(const YAML::Node & node) noexcept
{
    if(!node or !node["key"]) return std::nullopt;
    std::string node_type = node["type"] ? node["type"].as<std::string>() : "bool";

    if(node_type == "bool") {
        return base_data_ref::ptr_type(new data_ref<bool>(node));
    }
    else if(node_type == "int") {
        return base_data_ref::ptr_type(new data_ref<int>(node));
    }
    return std::nullopt;
}

value_data_ref::value_data_ref(const YAML::Node & node) noexcept {
    if(!node or node.IsSequence() == false) return;
    for(const auto & value : node) {
        auto data = make_data_ref(value);
        if(data.has_value()) data_.emplace_back(std::move(data.value()));
    }
}

autopilot_data_ref::autopilot_data_ref(const YAML::Node & node) noexcept :
    hdg_(node["hdg"] ? std::optional(value_data_ref(node["hdg"])) : std::nullopt),
    nav_(node["nav"] ? std::optional(value_data_ref(node["nav"])) : std::nullopt),
    apr_(node["apr"] ? std::optional(value_data_ref(node["apr"])) : std::nullopt),
    rev_(node["rev"] ? std::optional(value_data_ref(node["rev"])) : std::nullopt),
    alt_(node["alt"] ? std::optional(value_data_ref(node["alt"])) : std::nullopt),
    vs_ (node["vs"] ? std::optional(value_data_ref(node["vs"])) : std::nullopt),
    ias_(node["ias"] ? std::optional(value_data_ref(node["ias"])) : std::nullopt),
    ap_(node["ap"])
{
}

std::expected<autopilot_data_ref, int>
autopilot_data_ref::build(const YAML::Node & node) noexcept
{
    // Only the AP annunciator is required
    if(!node["ap"]) return std::unexpected(1);
    return autopilot_data_ref(node);
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
    YAML::Node node = YAML::LoadFile(path);
    if(!node["name"]) return std::unexpected(0);
    if(!node["models"] or node["models"].IsSequence() == false) return std::unexpected(0);
    std::vector<std::string> models;
    for(const auto & model : node["models"]) {
        if(node.Type() != YAML::NodeType::Scalar) return std::unexpected(0);
        models.emplace_back(node.as<std::string>());
    }
    if(!node["system"]) return std::unexpected(0);
    auto system = system_data_ref::build(node["system"]);
    if(system.has_value() == false) return std::unexpected(0);

    std::optional<autopilot_data_ref> autopilot;
    if(node["autopilot"]) {
        auto ap_ret = autopilot_data_ref::build(node["autopilot"]);
        if(ap_ret.has_value() == false) return std::unexpected(0);
        autopilot = std::move(ap_ret.value());
    }

    std::optional<annunciator_data_ref> annunciator;
    if(node["annunciator"]) {
        auto ann_ret = annunciator_data_ref::build(node["annunciator"]);
        if(ann_ret.has_value() == false) return std::unexpected(0);
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