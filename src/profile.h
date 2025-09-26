#ifndef PROFILE_H_
#define PROFILE_H_

#include <XPLM/XPLMDataAccess.h>
#include <yaml.h>

#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <vector>

class base_data_ref {
protected:
    XPLMDataRef data_ref_;
public:
    using ptr_type =std::unique_ptr<base_data_ref>;

    base_data_ref(const YAML::Node & node) noexcept;
    virtual
    ~base_data_ref() noexcept;

    virtual
    bool
    is_set() const = 0;

#if defined(HCBRAVO_PROFILE_TESTS)
    inline
    const XPLMDataRef &
    data_ref() const noexcept { return this->data_ref_; }
#endif
};

template<typename T>
class data_ref;

class value_data_ref {
    std::vector<base_data_ref::ptr_type> data_;
public:
    value_data_ref(const YAML::Node & node) noexcept;

    inline 
    bool
    is_set() const noexcept {
        for(const auto & data : this->data_ ) {
            if(data->is_set()) return true;
        }
        return false;
    }

#if defined(HCBRAVO_PROFILE_TESTS)
    inline
    const std::vector<base_data_ref::ptr_type> &
    data() const noexcept { return this->data_; }
#endif

};


class autopilot_data_ref {
    std::optional<value_data_ref> hdg_;
    std::optional<value_data_ref> nav_;
    std::optional<value_data_ref> apr_;
    std::optional<value_data_ref> rev_;
    std::optional<value_data_ref> alt_;
    std::optional<value_data_ref> vs_;
    std::optional<value_data_ref> ias_;
    value_data_ref ap_;

public:
    autopilot_data_ref(const YAML::Node & node) noexcept;

    static
    std::expected<autopilot_data_ref, int>
    build(const YAML::Node & node) noexcept;

    inline
    std::optional<bool>
    hdg() const noexcept {
        return this->hdg_.transform(&value_data_ref::is_set);
    }

    inline
    std::optional<bool>
    nav() const noexcept {
        return this->nav_.transform(&value_data_ref::is_set);
    }

    inline 
    std::optional<bool>
    apr() const noexcept {
        return this->apr_.transform(&value_data_ref::is_set);
    }

    inline
    std::optional<bool>
    rev() const noexcept {
        return this->rev_.transform(&value_data_ref::is_set);
    }

    inline 
    std::optional<bool>
    alt() const noexcept {
        return this->alt_.transform(&value_data_ref::is_set);
    }

    inline 
    std::optional<bool>
    vs() const noexcept {
        return this->vs_.transform(&value_data_ref::is_set);
    }

    inline
    std::optional<bool>
    ias() const noexcept {
        return this->ias_.transform(&value_data_ref::is_set);
    }

    inline 
    bool 
    ap() const noexcept { return this->ap_.is_set(); }

#if defined(HCBRAVO_PROFILE_TESTS)
    inline
    const std::optional<value_data_ref> &
    hdg_data_ref() const noexcept { return this->hdg_; }

    inline
    const std::optional<value_data_ref> &
    nav_data_ref() const noexcept { return this->nav_; }

    inline
    const std::optional<value_data_ref> &
    apr_data_ref() const noexcept { return this->apr_; }

    inline
    const std::optional<value_data_ref> &
    rev_data_ref() const noexcept { return this->rev_; }

    inline
    const std::optional<value_data_ref> &
    alt_data_ref() const noexcept { return this->alt_; }

    inline
    const std::optional<value_data_ref> &
    vs_data_ref() const noexcept { return this->vs_; }

    inline
    const std::optional<value_data_ref> &
    ias_data_ref() const noexcept { return this->ias_; }

    inline
    const value_data_ref &
    ap_data_ref() const noexcept { return this->ap_; }
#endif

};

class system_data_ref {
    value_data_ref volts_;
    std::optional<value_data_ref> gear_;

public:

    system_data_ref(const YAML::Node & node) noexcept;

    static
    std::expected<system_data_ref, int>
    build(const YAML::Node & node) noexcept;

    inline 
    bool 
    volts() const noexcept { return this->volts_.is_set(); }

    inline 
    std::optional<bool> 
    gear() const noexcept {
        return this->gear_.transform([] (const auto & v) {
            return v.is_set();
        });
    }

#if defined(HCBRAVO_PROFILE_TESTS)
    inline
    const value_data_ref &
    volts_data_ref() const noexcept { return this->volts_; }

    inline
    const std::optional<value_data_ref> &
    gear_data_ref() const noexcept { return this->gear_; }
#endif
};

class annunciator_data_ref {
    value_data_ref master_warn_;
    value_data_ref eng_fire_;
    value_data_ref oil_low_;
    value_data_ref fuel_low_;
    value_data_ref anti_ice_;
    value_data_ref starter_;
    std::optional<value_data_ref> apu_;
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
    const std::optional<bool>
    apu() const noexcept {
        return this->apu_.transform([] (const auto & v) {
            return v.is_set();
        });
    }

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