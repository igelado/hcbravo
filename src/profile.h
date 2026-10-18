// SPDX-License-Identifier: LGPL-2.1-only
//
// Runtime X-Plane dataref bindings built from conf objects.

#ifndef PROFILE_H_
#define PROFILE_H_

#include <XPLM/XPLMDataAccess.h>

#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace conf {
class key;
class value;
class airspeed;
class autopilot_dial;
class autopilot_mode;
class autopilot;
class gear_signals;
class gear;
class system;
class annunciator;
class configuration;
}

namespace profile {

enum class gear_state { safe, unsafe, up };

class base_data_ref {
protected:
    struct binding {
        XPLMDataRef data_ref;
        bool invert;
        std::optional<int> index;
        XPLMDataTypeID type;
        std::string path;
    };

    XPLMDataRef data_ref_;
    bool invert_;
    std::optional<int> index_;
    std::string path_;

    base_data_ref(binding && binding) noexcept;
    enum class numeric_type { any, floating };
    static std::expected<binding, int> bind(const conf::key & key, numeric_type numeric = numeric_type::any);

public:
    base_data_ref(base_data_ref &&) noexcept = default;
    base_data_ref & operator=(base_data_ref &&) noexcept = default;

#if defined(HCBRAVO_PROFILE_TESTS)
    const XPLMDataRef & data_ref() const noexcept { return data_ref_; }
#endif
};

class bool_data_ref final : public base_data_ref {
    enum class type { integer, floating };
    type type_;
    std::vector<int> integer_values_;
    std::vector<float> float_values_;

    struct sample {
        double raw;
        int count;
        bool result;
    };
    sample read() const noexcept;

    bool_data_ref(binding && binding, std::vector<int> && values) noexcept;
    bool_data_ref(binding && binding, std::vector<float> && values) noexcept;

public:
    using ptr_type = std::unique_ptr<bool_data_ref>;

    static std::expected<ptr_type, int> build(const conf::key & key);
    bool is_set() const noexcept;
#if !defined(NDEBUG)
    bool dump_state() const noexcept;
#endif
};

class float_data_ref final : public base_data_ref {
    // Set once a failed indexed read was logged; cleared by the next success so a
    // persistent failure does not flood the log every knob event.
    mutable bool read_failure_logged_ = false;
    float_data_ref(binding && binding) noexcept;

public:
    static std::expected<float_data_ref, int> build(const conf::key & key);
    float get() const noexcept;
    void set(float value) const noexcept;
#if !defined(NDEBUG)
    void dump_state(const char * label) const noexcept;
#endif
};

class value_data_ref {
    std::vector<bool_data_ref::ptr_type> data_;
    explicit value_data_ref(std::vector<bool_data_ref::ptr_type> && data) noexcept;

public:
    static std::expected<value_data_ref, int> build(const conf::value & value);
    bool is_set() const noexcept;
    bool empty() const noexcept { return data_.empty(); }
#if !defined(NDEBUG)
    bool dump_state(const char * label) const noexcept;
#endif

#if defined(HCBRAVO_PROFILE_TESTS)
    const std::vector<bool_data_ref::ptr_type> & data() const noexcept { return data_; }
#endif
};

enum class airspeed_unit { Knots, Mach };

class airspeed_data_ref {
    bool_data_ref::ptr_type is_mach_;
    float_data_ref value_;
    airspeed_data_ref(bool_data_ref::ptr_type && is_mach, float_data_ref && value) noexcept;

public:
    static std::expected<airspeed_data_ref, int> build(const conf::airspeed & airspeed);
    airspeed_unit unit() const noexcept;
    float get() const noexcept { return value_.get(); }
    void set(float value) const noexcept { value_.set(value); }
#if !defined(NDEBUG)
    void dump_state() const noexcept;
#endif
};

class autopilot_dial_data_ref {
    std::optional<airspeed_data_ref> ias_;
    std::optional<float_data_ref> course_;
    std::optional<float_data_ref> heading_;
    std::optional<float_data_ref> vs_;
    std::optional<float_data_ref> alt_;

    autopilot_dial_data_ref(
        std::optional<airspeed_data_ref> && ias,
        std::optional<float_data_ref> && course,
        std::optional<float_data_ref> && heading,
        std::optional<float_data_ref> && vs,
        std::optional<float_data_ref> && alt
    ) noexcept;

public:
    static std::expected<autopilot_dial_data_ref, int> build(const conf::autopilot_dial & dials);
    const std::optional<airspeed_data_ref> & ias() const noexcept { return ias_; }
    const std::optional<float_data_ref> & course() const noexcept { return course_; }
    const std::optional<float_data_ref> & heading() const noexcept { return heading_; }
    const std::optional<float_data_ref> & vs() const noexcept { return vs_; }
    const std::optional<float_data_ref> & alt() const noexcept { return alt_; }
#if !defined(NDEBUG)
    void dump_state() const noexcept;
#endif
};

class autopilot_mode_data_ref {
    std::optional<value_data_ref> hdg_;
    std::optional<value_data_ref> nav_;
    std::optional<value_data_ref> apr_;
    std::optional<value_data_ref> rev_;
    std::optional<value_data_ref> alt_;
    std::optional<value_data_ref> vs_;
    std::optional<value_data_ref> ias_;
    value_data_ref ap_;

    autopilot_mode_data_ref(
        std::optional<value_data_ref> && hdg, std::optional<value_data_ref> && nav,
        std::optional<value_data_ref> && apr, std::optional<value_data_ref> && rev,
        std::optional<value_data_ref> && alt, std::optional<value_data_ref> && vs,
        std::optional<value_data_ref> && ias, value_data_ref && ap
    ) noexcept;

public:
    static std::expected<autopilot_mode_data_ref, int> build(const conf::autopilot_mode & modes);
    std::optional<bool> hdg() const noexcept;
    std::optional<bool> nav() const noexcept;
    std::optional<bool> apr() const noexcept;
    std::optional<bool> rev() const noexcept;
    std::optional<bool> alt() const noexcept;
    std::optional<bool> vs() const noexcept;
    std::optional<bool> ias() const noexcept;
    bool ap() const noexcept { return ap_.is_set(); }
#if !defined(NDEBUG)
    void dump_state() const noexcept;
#endif
};

class autopilot_data_ref {
    autopilot_mode_data_ref mode_;
    std::optional<autopilot_dial_data_ref> dials_;
    autopilot_data_ref(autopilot_mode_data_ref && mode,
                       std::optional<autopilot_dial_data_ref> && dials) noexcept;

public:
    static std::expected<autopilot_data_ref, int> build(const conf::autopilot & autopilot);
    const autopilot_mode_data_ref & mode() const noexcept { return mode_; }
    const std::optional<autopilot_dial_data_ref> & dials() const noexcept { return dials_; }
#if !defined(NDEBUG)
    void dump_state() const noexcept;
#endif
};

class gear_signals_data_ref {
    value_data_ref safe_;
    value_data_ref unsafe_;
    gear_signals_data_ref(value_data_ref && safe, value_data_ref && unsafe) noexcept;

public:
    static std::expected<gear_signals_data_ref, int> build(const conf::gear_signals & signals);
    gear_state state() const noexcept;
#if !defined(NDEBUG)
    gear_state dump_state(const char * label) const noexcept;
#endif
};

class gear_data_ref {
    gear_signals_data_ref left_;
    gear_signals_data_ref nose_;
    gear_signals_data_ref right_;
    gear_data_ref(gear_signals_data_ref && left, gear_signals_data_ref && nose,
                  gear_signals_data_ref && right) noexcept;

public:
    static std::expected<gear_data_ref, int> build(const conf::gear & gear);
    gear_state left_state() const noexcept { return left_.state(); }
    gear_state nose_state() const noexcept { return nose_.state(); }
    gear_state right_state() const noexcept { return right_.state(); }
    bool left() const noexcept { return left_state() == gear_state::safe; }
    bool nose() const noexcept { return nose_state() == gear_state::safe; }
    bool right() const noexcept { return right_state() == gear_state::safe; }
#if !defined(NDEBUG)
    void dump_state() const noexcept;
#endif
};

class system_data_ref {
    value_data_ref volts_;
    std::optional<gear_data_ref> gear_;
    system_data_ref(value_data_ref && volts, std::optional<gear_data_ref> && gear) noexcept;

public:
    static std::expected<system_data_ref, int> build(const conf::system & system);
    bool volts() const noexcept { return volts_.is_set(); }
    const std::optional<gear_data_ref> & gear() const noexcept { return gear_; }
#if !defined(NDEBUG)
    void dump_state() const noexcept;
#endif
};

class annunciator_data_ref {
    std::optional<value_data_ref> master_warn_, eng_fire_, oil_low_, fuel_low_, anti_ice_, starter_, apu_;
    std::optional<value_data_ref> master_caution_, vacuum_low_, hydro_low_, aux_fuel_, parking_brake_;
    std::optional<value_data_ref> volt_low_, door_open_;

    annunciator_data_ref(
        std::optional<value_data_ref> && master_warn, std::optional<value_data_ref> && eng_fire,
        std::optional<value_data_ref> && oil_low, std::optional<value_data_ref> && fuel_low,
        std::optional<value_data_ref> && anti_ice, std::optional<value_data_ref> && starter,
        std::optional<value_data_ref> && apu, std::optional<value_data_ref> && master_caution,
        std::optional<value_data_ref> && vacuum_low, std::optional<value_data_ref> && hydro_low,
        std::optional<value_data_ref> && aux_fuel, std::optional<value_data_ref> && parking_brake,
        std::optional<value_data_ref> && volt_low, std::optional<value_data_ref> && door_open
    ) noexcept;

public:
    static std::expected<annunciator_data_ref, int> build(const conf::annunciator & annunciator);
    std::optional<bool> master_warn() const noexcept;
    std::optional<bool> eng_fire() const noexcept;
    std::optional<bool> oil_low() const noexcept;
    std::optional<bool> fuel_low() const noexcept;
    std::optional<bool> anti_ice() const noexcept;
    std::optional<bool> starter() const noexcept;
    std::optional<bool> apu() const noexcept;
    std::optional<bool> master_caution() const noexcept;
    std::optional<bool> vacuum_low() const noexcept;
    std::optional<bool> hydro_low() const noexcept;
    std::optional<bool> aux_fuel() const noexcept;
    std::optional<bool> parking_brake() const noexcept;
    std::optional<bool> volt_low() const noexcept;
    std::optional<bool> door_open() const noexcept;
#if !defined(NDEBUG)
    void dump_state() const noexcept;
#endif
};

class configuration {
    system_data_ref system_;
    std::optional<autopilot_data_ref> autopilot_;
    std::optional<annunciator_data_ref> annunciator_;
    configuration(system_data_ref && system, std::optional<autopilot_data_ref> && autopilot,
                  std::optional<annunciator_data_ref> && annunciator) noexcept;

public:
    using ptr_type = std::shared_ptr<configuration>;
    static std::expected<ptr_type, int> build(const conf::configuration & configuration);
    const system_data_ref & system() const noexcept { return system_; }
    const std::optional<autopilot_data_ref> & autopilot() const noexcept { return autopilot_; }
    const std::optional<annunciator_data_ref> & annunciator() const noexcept { return annunciator_; }
};

} // namespace profile

#endif
