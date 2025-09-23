#ifndef PROFILE_H_
#define PROFILE_H_

#include <XPLM/XPLMDataAccess.h>
#include <yaml.h>

#include <string>
#include <vector>

struct data_ref {
    XPLMDataRef data_ref_;
    std::vector<unsigned> values_;

    data_ref(const YAML::Node & node) noexcept;
};

struct value_data_ref {
    std::vector<data_ref> data_;
    value_data_ref(const YAML::Node & node) noexcept;
};



struct autopilot_data_ref {
    value_data_ref hdg_;
    value_data_ref nav_;
    value_data_ref apr_;
    value_data_ref rev_;
    value_data_ref alt_;
    value_data_ref vs_;
    value_data_ref ias_;
    value_data_ref ap_;

    autopilot_data_ref(const YAML::Node & node) noexcept;
};

struct system_data_ref {
    value_data_ref volts_;
    value_data_ref gear_;

    system_data_ref(const YAML::Node & node) noexcept;
};

struct annunciator_data_ref {
    value_data_ref master_warn_;
    value_data_ref eng_fire_;
    value_data_ref oil_low_;
    value_data_ref fuel_low_;
    value_data_ref anti_ice_;
    value_data_ref starter_;
    value_data_ref apu_;
    value_data_ref master_caution_;
    value_data_ref vacuum_low_;
    value_data_ref hydro_low_;
    value_data_ref aux_fuel_;
    value_data_ref parking_brake_;
    value_data_ref volt_low_;
    value_data_ref door_open_;

    annunciator_data_ref(const YAML::Node & node) noexcept;
};


class profile {
protected:
    std::string name_;
    std::vector<std::string> models_;
    autopilot_data_ref autopilot_;
    system_data_ref system_;
    annunciator_data_ref annunciator_;

    profile(const YAML::Node & node) noexcept;
public:
    static profile from_yaml(const std::string & path) noexcept;

    inline
    const std::string &
    name() const { return this->name_; }

    inline
    const std::vector<std::string> &
    models() const { return this->models_; }
};

#endif