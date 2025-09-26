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
    master_warn_(node["master_warn"]),
    eng_fire_(node["eng_fire"]),
    oil_low_(node["oil_low"]),
    fuel_low_(node["fuel_low"]),
    anti_ice_(node["anti_ice"]),
    starter_(node["starter"]),
    apu_(node["apu"] ? std::optional(value_data_ref(node["apu"])) : std::nullopt),
    master_caution_(node["master_caution"]),
    vacuum_low_(node["vacuum_low"]),
    hydro_low_(node["hydro_low"]),
    aux_fuel_(node["aux_fuel"]),
    parking_brake_(node["parking_brake"]),
    volt_low_(node["volt_low"]),
    door_open_(node["door_open"])
{}


profile::profile(const YAML::Node & node) noexcept :
    name_(node["name"].as<std::string>()),
    models_(node["models"].as<std::vector<std::string>>()),
    autopilot_(node["autopilot"]),
    system_(node["system"]),
    annunciator_(node["annunciator"])
{}

profile::ptr_type
profile::from_yaml(const std::string & path) noexcept {
    YAML::Node node = YAML::LoadFile(path);
    return ptr_type(new profile(node));
}