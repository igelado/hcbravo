#ifndef PROFILE_H_
#define PROFILE_H_

#include <XPLM/XPLMDataAccess.h>
#include <yaml.h>

#include <memory>
#include <string>
#include <vector>

class base_data_ref {
protected:
    XPLMDataRef data_ref_;
public:
    using ptr_type =std::unique_ptr<base_data_ref>;

    base_data_ref(const YAML::Node & node) noexcept;

    inline
    const XPLMDataRef &
    data_ref() const noexcept { return this->data_ref_; }

    virtual
    bool
    is_set() const = 0;
};

template<typename T>
class data_ref;

class value_data_ref {
    std::vector<base_data_ref::ptr_type> data_;
public:
    value_data_ref(const YAML::Node & node) noexcept;

    inline
    const std::vector<base_data_ref::ptr_type> &
    data() const noexcept { return this->data_; }

    inline 
    bool
    is_set() const noexcept {
        for(const auto & data : this->data_ ) {
            if(data->is_set()) return true;
        }
        return false;
    }
};



class autopilot_data_ref {
    value_data_ref hdg_;
    value_data_ref nav_;
    value_data_ref apr_;
    value_data_ref rev_;
    value_data_ref alt_;
    value_data_ref vs_;
    value_data_ref ias_;
    value_data_ref ap_;

    autopilot_data_ref(const YAML::Node & node) noexcept;

    friend class profile;
public:
    inline
    bool
    hdg() const noexcept { return this->hdg_.is_set(); }

    inline
    bool
    nav() const noexcept { return this->nav_.is_set(); }

    inline 
    bool
    apr() const noexcept { return this->apr_.is_set(); }

    inline
    bool
    rev() const noexcept { return this->rev_.is_set(); }

    inline 
    bool
    alt() const noexcept { return this->alt_.is_set(); }

    inline 
    bool 
    vs() const noexcept { return this->vs_.is_set(); }

    inline
    bool 
    ias() const noexcept { return this->ias_.is_set(); }

    inline 
    bool 
    ap() const noexcept { return this->ap_.is_set(); }
};

class system_data_ref {
    value_data_ref volts_;
    value_data_ref gear_;

    system_data_ref(const YAML::Node & node) noexcept;

    friend class profile;
public:

    inline 
    bool 
    volts() const noexcept { return this->volts_.is_set(); }

    inline 
    bool 
    gear() const noexcept { return this->gear_.is_set(); }
};

class annunciator_data_ref {
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

    friend class profile;
public:

    inline 
    bool 
    master_warn() const noexcept { return this->master_warn_.is_set(); }

    inline 
    bool 
    eng_fire() const noexcept { return this->eng_fire_.is_set(); }

    inline 
    bool 
    oil_low() const noexcept { return this->oil_low_.is_set(); }

    inline 
    bool 
    fuel_low() const noexcept { return this->fuel_low_.is_set(); }

    inline 
    bool 
    anti_ice() const noexcept { return this->anti_ice_.is_set(); }

    inline 
    bool 
    starter() const noexcept { return this->starter_.is_set(); }

    inline 
    bool
    apu() const noexcept { return this->apu_.is_set(); }

    inline 
    bool 
    master_caution() const noexcept { return this->master_caution_.is_set(); }

    inline 
    bool 
    vacuum_low() const noexcept { return this->vacuum_low_.is_set(); }

    inline 
    bool 
    hydro_low() const noexcept { return this->hydro_low_.is_set(); }
    
    inline
    bool
    aux_fuel() const noexcept { return this->aux_fuel_.is_set(); }

    inline
    bool
    parking_brake() const noexcept { return this->parking_brake_.is_set(); }

    inline
    bool
    volt_low() const noexcept { return this->volt_low_.is_set(); }
    
    inline
    bool
    door_open() const noexcept { return this->door_open_.is_set(); }
};


class profile {
public:
    using ptr_type = std::shared_ptr<profile>;
protected:
    std::string name_;
    std::vector<std::string> models_;
    autopilot_data_ref autopilot_;
    system_data_ref system_;
    annunciator_data_ref annunciator_;

    profile(const YAML::Node & node) noexcept;
public:
    static ptr_type from_yaml(const std::string & path) noexcept;

    inline
    const std::string &
    name() const { return this->name_; }

    inline
    const std::vector<std::string> &
    models() const { return this->models_; }

    inline 
    const autopilot_data_ref &
    autopilot() const { return this->autopilot_; }

    inline 
    const system_data_ref &
    system() const { return this->system_; }

    inline
    const annunciator_data_ref &
    annunciator() const { return this->annunciator_; }
};

using profile_ptr = profile::ptr_type;

#endif