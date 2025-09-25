#include "profile.h"

#include <memory>
#include <optional>

base_data_ref::base_data_ref(const YAML::Node & node) noexcept :
    data_ref_(nullptr)
{
    if(!node) return;
    data_ref_ = XPLMFindDataRef(node["key"].as<std::string>().c_str());
}

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
    if(!node) return std::nullopt;
    std::string node_type = node["type"] ? "bool" : node["type"].as<std::string>();
    if(node_type == "bool") {
        return base_data_ref::ptr_type(new data_ref<bool>(node));
    }
    else if(node_type == "int") {
        return base_data_ref::ptr_type(new data_ref<int>(node));
    }
    return std::nullopt;
}

value_data_ref::value_data_ref(const YAML::Node & node) noexcept {
    if(!node) return;
    for(const auto & value : node) {
        auto value = make_data_ref(node);
        if(value.has_value()) data_.emplace_back(std::move(value.value()));
    }
}

autopilot_data_ref::autopilot_data_ref(const YAML::Node & node) noexcept :
    hdg_(node["hdg"]),
    nav_(node["nav"]),
    apr_(node["apr"]),
    rev_(node["rev"]),
    alt_(node["alt"]),
    vs_(node["vs"]),
    ias_(node["ias"]),
    ap_(node["ap"])
{
}

system_data_ref::system_data_ref(const YAML::Node & node) noexcept :
    volts_(node["volts"]),
    gear_(node["gear"])
{}

annunciator_data_ref::annunciator_data_ref(const YAML::Node & node) noexcept :
    master_warn_(node["master_warn"]),
    eng_fire_(node["eng_fire"]),
    oil_low_(node["oil_low"]),
    fuel_low_(node["fuel_low"]),
    anti_ice_(node["anti_ice"]),
    starter_(node["starter"]),
    apu_(node["apu"]),
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