// SPDX-License-Identifier: LGPL-2.1-only
//
// Runtime X-Plane dataref bindings built from conf objects.

#include "profile.h"

#include "conf.h"
#include "logger.h"

#include <XPLM/XPLMUtilities.h>

#include <string>
#include <utility>

namespace profile {

static bool has_type(XPLMDataTypeID type, XPLMDataTypeID expected) noexcept
{
    return (type & expected) != 0;
}

base_data_ref::base_data_ref(binding && binding) noexcept :
    data_ref_(std::move(binding.data_ref)),
    invert_(binding.invert),
    index_(binding.index),
    path_(std::move(binding.path))
{}

std::expected<base_data_ref::binding, int>
base_data_ref::bind(const conf::key & key, numeric_type numeric)
{
    auto data_ref = XPLMFindDataRef(key.path().c_str());
    if(data_ref == nullptr) {
        logger::info() << "DataRef not found: '" << key.path() << "'";
        return std::unexpected(0);
    }

    XPLMDataRefInfo_t info {};
    info.structSize = sizeof(info);
    XPLMGetDataRefInfo(data_ref, &info);

    auto index = key.index();
    const auto select_type = [&](bool array) {
        const auto integer = array ? xplmType_IntArray : xplmType_Int;
        const auto floating = array ? xplmType_FloatArray : xplmType_Float;
        if(numeric == numeric_type::any && has_type(info.type, integer)) return integer;
        if(has_type(info.type, floating)) return floating;
        return xplmType_Unknown;
    };
    // An explicit index prefers an array; otherwise prefer a supported scalar.
    // Select the numeric accessor and its shape together so mixed type masks
    // never result in an unsupported integer-array or float-array read.
    auto type = select_type(index.has_value());
    if(type == xplmType_Unknown) type = select_type(!index.has_value());
    if(type == xplmType_Unknown) {
        logger::info() << "DataRef '" << key.path() << "' has no supported "
                 << (numeric == numeric_type::floating ? "float" : "numeric") << " accessor";
        return std::unexpected(0);
    }
    const bool is_array = type == xplmType_IntArray || type == xplmType_FloatArray;
    if(is_array && !index) {
        logger::info() << "Array DataRef '" << key.path() << "' has no index; using index 0";
        index = 0;
    }
    else if(!is_array && index) {
        logger::info() << "Scalar DataRef '" << key.path() << "' ignores index " << *index;
        index = std::nullopt;
    }

#if !defined(NDEBUG)
    if(logger::diagnostics::enabled(logger::diagnostic_category::datarefs)) {
        logger::debug log;
        log << "DataRef bound: '" << key.path() << "' type_mask=" << info.type
            << " accessor_type=" << type
            << " index=" << (index ? std::to_string(*index) : "scalar")
            << " invert=" << key.inverted() << " values=[";
        for(const auto & value : key.values()) log << value << " ";
        log << "] (empty values means nonzero)";
    }
#endif

    return binding { std::move(data_ref), key.inverted(), index, type, key.path() };
}

bool_data_ref::bool_data_ref(binding && binding, std::vector<int> && values) noexcept :
    base_data_ref(std::move(binding)),
    type_(type::integer),
    integer_values_(std::move(values))
{}

bool_data_ref::bool_data_ref(binding && binding, std::vector<float> && values) noexcept :
    base_data_ref(std::move(binding)),
    type_(type::floating),
    float_values_(std::move(values))
{}

std::expected<bool_data_ref::ptr_type, int>
bool_data_ref::build(const conf::key & key)
{
    auto binding = base_data_ref::bind(key);
    if(!binding) return std::unexpected(binding.error());

    if(has_type(binding->type, xplmType_Int) || has_type(binding->type, xplmType_IntArray)) {
        std::vector<int> values;
        try {
            for(const auto & value : key.values()) {
                size_t parsed = 0;
                const int number = std::stoi(value, &parsed);
                if(parsed != value.size()) throw std::invalid_argument("trailing characters");
                values.emplace_back(number);
            }
        }
        catch(const std::exception &) {
            logger::info() << "DataRef '" << key.path() << "' has a value incompatible with its integer type";
            return std::unexpected(0);
        }
        return ptr_type(new bool_data_ref(std::move(binding.value()), std::move(values)));
    }

    if(has_type(binding->type, xplmType_Float) || has_type(binding->type, xplmType_FloatArray)) {
        std::vector<float> values;
        try {
            for(const auto & value : key.values()) {
                size_t parsed = 0;
                const float number = std::stof(value, &parsed);
                if(parsed != value.size()) throw std::invalid_argument("trailing characters");
                values.emplace_back(number);
            }
        }
        catch(const std::exception &) {
            logger::info() << "DataRef '" << key.path() << "' has a value incompatible with its float type";
            return std::unexpected(0);
        }
        return ptr_type(new bool_data_ref(std::move(binding.value()), std::move(values)));
    }

    logger::info() << "DataRef '" << key.path() << "' has an unsupported type";
    return std::unexpected(0);
}

bool
bool_data_ref::is_set() const noexcept
{
    return read().result;
}

bool_data_ref::sample
bool_data_ref::read() const noexcept
{
    if(type_ == type::integer) {
        int value = 0;
        int count = 1;
        if(index_) count = XPLMGetDatavi(data_ref_, &value, *index_, 1);
        else value = XPLMGetDatai(data_ref_);

        bool result = value != 0;
        if(!integer_values_.empty()) {
            result = false;
            for(const auto expected : integer_values_) {
                if(value == expected) {
                    result = true;
                    break;
                }
            }
        }
        // A failed indexed read has no value to invert; never report it as set.
        if(count != 1) result = false;
        else result = invert_ ? !result : result;
        return {static_cast<double>(value), count, result};
    }

    float value = 0.0f;
    int count = 1;
    if(index_) count = XPLMGetDatavf(data_ref_, &value, *index_, 1);
    else value = XPLMGetDataf(data_ref_);

    bool result = value != 0.0f;
    if(!float_values_.empty()) {
        result = false;
        for(const auto expected : float_values_) {
            if(value == expected) {
                result = true;
                break;
            }
        }
    }
    // A failed indexed read has no value to invert; never report it as set.
    if(count != 1) result = false;
    else result = invert_ ? !result : result;
    return {value, count, result};
}

#if !defined(NDEBUG)
bool
bool_data_ref::dump_state() const noexcept
{
    const auto value = read();
    logger::debug log;
    log << "[DataRef debug] DataRef '" << path_ << "' index="
             << (index_ ? std::to_string(*index_) : "scalar")
             << " reader=" << (type_ == type::integer ? "int" : "float")
             << " raw=" << value.raw << " count=" << value.count
             << " invert=" << invert_ << " result=" << value.result
             << (value.count == 1 ? "" : " READ FAILED; using zero-initialized value")
             << " values=[";
    if(type_ == type::integer) {
        for(const auto expected : integer_values_) log << expected << " ";
    }
    else {
        for(const auto expected : float_values_) log << expected << " ";
    }
    log << "] (empty values means nonzero)";
    return value.result;
}
#endif

float_data_ref::float_data_ref(binding && binding) noexcept :
    base_data_ref(std::move(binding))
{}

std::expected<float_data_ref, int>
float_data_ref::build(const conf::key & key)
{
    auto binding = base_data_ref::bind(key, numeric_type::floating);
    if(!binding) return std::unexpected(binding.error());
    if(!has_type(binding->type, xplmType_Float) && !has_type(binding->type, xplmType_FloatArray)) {
        logger::info() << "DataRef '" << key.path() << "' is not a float DataRef";
        return std::unexpected(0);
    }
    return float_data_ref(std::move(binding.value()));
}

float
float_data_ref::get() const noexcept
{
    float value = 0.0f;
    if(index_) {
        if(XPLMGetDatavf(data_ref_, &value, *index_, 1) != 1) {
            logger::info() << "Failed to read indexed float DataRef at " << *index_;
        }
    }
    else value = XPLMGetDataf(data_ref_);
#if !defined(NDEBUG)
    if(logger::diagnostic_scope::enabled(logger::diagnostic_category::datarefs)) {
        logger::debug() << "[DataRef debug] Float read '" << path_ << "' raw=" << value;
    }
#endif
    return value;
}

void
float_data_ref::set(float value) const noexcept
{
    if(index_) XPLMSetDatavf(data_ref_, &value, *index_, 1);
    else XPLMSetDataf(data_ref_, value);
#if !defined(NDEBUG)
    if(logger::diagnostic_scope::enabled(logger::diagnostic_category::datarefs)) {
        logger::debug() << "[DataRef debug] Float write '" << path_ << "' value=" << value;
    }
#endif
}

value_data_ref::value_data_ref(std::vector<bool_data_ref::ptr_type> && data) noexcept :
    data_(std::move(data))
{}

std::expected<value_data_ref, int>
value_data_ref::build(const conf::value & value)
{
    std::vector<bool_data_ref::ptr_type> data;
    for(const auto & key : value.keys()) {
        auto bound = bool_data_ref::build(key);
        if(!bound) return std::unexpected(bound.error());
        data.emplace_back(std::move(bound.value()));
    }
    if(data.empty()) {
        logger::info() << "DataRef value has no entries";
        return std::unexpected(0);
    }
    return value_data_ref(std::move(data));
}

bool
value_data_ref::is_set() const noexcept
{
    for(const auto & data : data_) {
        if(data->is_set()) return true;
    }
    return false;
}

airspeed_data_ref::airspeed_data_ref(bool_data_ref::ptr_type && is_mach,
                                     float_data_ref && value) noexcept :
    is_mach_(std::move(is_mach)),
    value_(std::move(value))
{}

std::expected<airspeed_data_ref, int>
airspeed_data_ref::build(const conf::airspeed & airspeed)
{
    auto is_mach = bool_data_ref::build(airspeed.is_mach());
    if(!is_mach) return std::unexpected(is_mach.error());
    if(airspeed.value().keys().size() != 1) {
        logger::info() << "IAS value must contain exactly one DataRef";
        return std::unexpected(0);
    }
    auto value = float_data_ref::build(airspeed.value().keys().front());
    if(!value) return std::unexpected(value.error());
    return airspeed_data_ref(std::move(is_mach.value()), std::move(value.value()));
}

airspeed_unit
airspeed_data_ref::unit() const noexcept
{
    return is_mach_->is_set() ? airspeed_unit::Mach : airspeed_unit::Knots;
}

static std::expected<float_data_ref, int>
build_float_value(const conf::value & value)
{
    if(value.keys().size() != 1) {
        logger::info() << "Autopilot dial value must contain exactly one DataRef";
        return std::unexpected(0);
    }
    return float_data_ref::build(value.keys().front());
}

static std::expected<std::optional<float_data_ref>, int>
build_optional_float_value(const std::optional<conf::value> & value)
{
    if(!value) return std::optional<float_data_ref> {};
    auto bound = build_float_value(*value);
    if(!bound) return std::unexpected(bound.error());
    return std::optional<float_data_ref>(std::move(bound.value()));
}

template<typename T>
static std::expected<std::optional<T>, int>
build_optional_value(const std::optional<conf::value> & value)
{
    if(!value) return std::optional<T> {};
    auto bound = T::build(*value);
    if(!bound) return std::unexpected(bound.error());
    return std::optional<T>(std::move(bound.value()));
}

autopilot_dial_data_ref::autopilot_dial_data_ref(
    std::optional<airspeed_data_ref> && ias, std::optional<float_data_ref> && course,
    std::optional<float_data_ref> && heading, std::optional<float_data_ref> && vs,
    std::optional<float_data_ref> && alt
) noexcept :
    ias_(std::move(ias)), course_(std::move(course)), heading_(std::move(heading)),
    vs_(std::move(vs)), alt_(std::move(alt))
{}

std::expected<autopilot_dial_data_ref, int>
autopilot_dial_data_ref::build(const conf::autopilot_dial & dials)
{
    std::optional<airspeed_data_ref> ias;
    if(dials.ias()) {
        auto bound = airspeed_data_ref::build(*dials.ias());
        if(!bound) return std::unexpected(bound.error());
        ias.emplace(std::move(bound.value()));
    }

    auto course = build_optional_float_value(dials.course());
    if(!course) return std::unexpected(course.error());
    auto heading = build_optional_float_value(dials.heading());
    if(!heading) return std::unexpected(heading.error());
    auto vs = build_optional_float_value(dials.vs());
    if(!vs) return std::unexpected(vs.error());
    auto alt = build_optional_float_value(dials.alt());
    if(!alt) return std::unexpected(alt.error());

    return autopilot_dial_data_ref(
        std::move(ias), std::move(course.value()), std::move(heading.value()),
        std::move(vs.value()), std::move(alt.value())
    );
}

autopilot_mode_data_ref::autopilot_mode_data_ref(
    std::optional<value_data_ref> && hdg, std::optional<value_data_ref> && nav,
    std::optional<value_data_ref> && apr, std::optional<value_data_ref> && rev,
    std::optional<value_data_ref> && alt, std::optional<value_data_ref> && vs,
    std::optional<value_data_ref> && ias, value_data_ref && ap
) noexcept :
    hdg_(std::move(hdg)), nav_(std::move(nav)), apr_(std::move(apr)), rev_(std::move(rev)),
    alt_(std::move(alt)), vs_(std::move(vs)), ias_(std::move(ias)), ap_(std::move(ap))
{}

std::expected<autopilot_mode_data_ref, int>
autopilot_mode_data_ref::build(const conf::autopilot_mode & modes)
{
    auto hdg = build_optional_value<value_data_ref>(modes.hdg());
    if(!hdg) return std::unexpected(hdg.error());
    auto nav = build_optional_value<value_data_ref>(modes.nav());
    if(!nav) return std::unexpected(nav.error());
    auto apr = build_optional_value<value_data_ref>(modes.apr());
    if(!apr) return std::unexpected(apr.error());
    auto rev = build_optional_value<value_data_ref>(modes.rev());
    if(!rev) return std::unexpected(rev.error());
    auto alt = build_optional_value<value_data_ref>(modes.alt());
    if(!alt) return std::unexpected(alt.error());
    auto vs = build_optional_value<value_data_ref>(modes.vs());
    if(!vs) return std::unexpected(vs.error());
    auto ias = build_optional_value<value_data_ref>(modes.ias());
    if(!ias) return std::unexpected(ias.error());
    auto ap = value_data_ref::build(modes.ap());
    if(!ap) return std::unexpected(ap.error());

    return autopilot_mode_data_ref(
        std::move(hdg.value()), std::move(nav.value()), std::move(apr.value()),
        std::move(rev.value()), std::move(alt.value()), std::move(vs.value()),
        std::move(ias.value()), std::move(ap.value())
    );
}

std::optional<bool> autopilot_mode_data_ref::hdg() const noexcept { return hdg_.transform(&value_data_ref::is_set); }
std::optional<bool> autopilot_mode_data_ref::nav() const noexcept { return nav_.transform(&value_data_ref::is_set); }
std::optional<bool> autopilot_mode_data_ref::apr() const noexcept { return apr_.transform(&value_data_ref::is_set); }
std::optional<bool> autopilot_mode_data_ref::rev() const noexcept { return rev_.transform(&value_data_ref::is_set); }
std::optional<bool> autopilot_mode_data_ref::alt() const noexcept { return alt_.transform(&value_data_ref::is_set); }
std::optional<bool> autopilot_mode_data_ref::vs() const noexcept { return vs_.transform(&value_data_ref::is_set); }
std::optional<bool> autopilot_mode_data_ref::ias() const noexcept { return ias_.transform(&value_data_ref::is_set); }

autopilot_data_ref::autopilot_data_ref(autopilot_mode_data_ref && mode,
                                       std::optional<autopilot_dial_data_ref> && dials) noexcept :
    mode_(std::move(mode)), dials_(std::move(dials))
{}

std::expected<autopilot_data_ref, int>
autopilot_data_ref::build(const conf::autopilot & autopilot)
{
    auto mode = autopilot_mode_data_ref::build(autopilot.mode());
    if(!mode) return std::unexpected(mode.error());
    std::optional<autopilot_dial_data_ref> dials;
    if(autopilot.dials()) {
        auto bound = autopilot_dial_data_ref::build(*autopilot.dials());
        if(!bound) return std::unexpected(bound.error());
        dials.emplace(std::move(bound.value()));
    }
    return autopilot_data_ref(std::move(mode.value()), std::move(dials));
}

gear_signals_data_ref::gear_signals_data_ref(value_data_ref && safe, value_data_ref && unsafe) noexcept :
    safe_(std::move(safe)), unsafe_(std::move(unsafe))
{}

std::expected<gear_signals_data_ref, int>
gear_signals_data_ref::build(const conf::gear_signals & signals)
{
    auto safe = value_data_ref::build(signals.safe());
    if(!safe) return std::unexpected(safe.error());
    auto unsafe = value_data_ref::build(signals.unsafe());
    if(!unsafe) return std::unexpected(unsafe.error());
    return gear_signals_data_ref(std::move(safe.value()), std::move(unsafe.value()));
}

gear_state
gear_signals_data_ref::state() const noexcept
{
    if(safe_.is_set()) return gear_state::safe;
    if(unsafe_.is_set()) return gear_state::unsafe;
    return gear_state::up;
}

gear_data_ref::gear_data_ref(gear_signals_data_ref && left, gear_signals_data_ref && nose,
                             gear_signals_data_ref && right) noexcept :
    left_(std::move(left)), nose_(std::move(nose)), right_(std::move(right))
{}

std::expected<gear_data_ref, int>
gear_data_ref::build(const conf::gear & gear)
{
    auto left = gear_signals_data_ref::build(gear.left());
    if(!left) return std::unexpected(left.error());
    auto nose = gear_signals_data_ref::build(gear.nose());
    if(!nose) return std::unexpected(nose.error());
    auto right = gear_signals_data_ref::build(gear.right());
    if(!right) return std::unexpected(right.error());
    return gear_data_ref(std::move(left.value()), std::move(nose.value()), std::move(right.value()));
}

system_data_ref::system_data_ref(value_data_ref && volts, std::optional<gear_data_ref> && gear) noexcept :
    volts_(std::move(volts)), gear_(std::move(gear))
{}

std::expected<system_data_ref, int>
system_data_ref::build(const conf::system & system)
{
    auto volts = value_data_ref::build(system.volts());
    if(!volts) return std::unexpected(volts.error());
    std::optional<gear_data_ref> gear;
    if(system.gear()) {
        auto bound = gear_data_ref::build(*system.gear());
        if(!bound) return std::unexpected(bound.error());
        gear.emplace(std::move(bound.value()));
    }
    return system_data_ref(std::move(volts.value()), std::move(gear));
}

annunciator_data_ref::annunciator_data_ref(
    std::optional<value_data_ref> && master_warn, std::optional<value_data_ref> && eng_fire,
    std::optional<value_data_ref> && oil_low, std::optional<value_data_ref> && fuel_low,
    std::optional<value_data_ref> && anti_ice, std::optional<value_data_ref> && starter,
    std::optional<value_data_ref> && apu, std::optional<value_data_ref> && master_caution,
    std::optional<value_data_ref> && vacuum_low, std::optional<value_data_ref> && hydro_low,
    std::optional<value_data_ref> && aux_fuel, std::optional<value_data_ref> && parking_brake,
    std::optional<value_data_ref> && volt_low, std::optional<value_data_ref> && door_open
) noexcept :
    master_warn_(std::move(master_warn)), eng_fire_(std::move(eng_fire)), oil_low_(std::move(oil_low)),
    fuel_low_(std::move(fuel_low)), anti_ice_(std::move(anti_ice)), starter_(std::move(starter)),
    apu_(std::move(apu)), master_caution_(std::move(master_caution)), vacuum_low_(std::move(vacuum_low)),
    hydro_low_(std::move(hydro_low)), aux_fuel_(std::move(aux_fuel)), parking_brake_(std::move(parking_brake)),
    volt_low_(std::move(volt_low)), door_open_(std::move(door_open))
{}

std::expected<annunciator_data_ref, int>
annunciator_data_ref::build(const conf::annunciator & annunciator)
{
    auto master_warn = build_optional_value<value_data_ref>(annunciator.master_warn());
    auto eng_fire = build_optional_value<value_data_ref>(annunciator.eng_fire());
    auto oil_low = build_optional_value<value_data_ref>(annunciator.oil_low());
    auto fuel_low = build_optional_value<value_data_ref>(annunciator.fuel_low());
    auto anti_ice = build_optional_value<value_data_ref>(annunciator.anti_ice());
    auto starter = build_optional_value<value_data_ref>(annunciator.starter());
    auto apu = build_optional_value<value_data_ref>(annunciator.apu());
    auto master_caution = build_optional_value<value_data_ref>(annunciator.master_caution());
    auto vacuum_low = build_optional_value<value_data_ref>(annunciator.vacuum_low());
    auto hydro_low = build_optional_value<value_data_ref>(annunciator.hydro_low());
    auto aux_fuel = build_optional_value<value_data_ref>(annunciator.aux_fuel());
    auto parking_brake = build_optional_value<value_data_ref>(annunciator.parking_brake());
    auto volt_low = build_optional_value<value_data_ref>(annunciator.volt_low());
    auto door_open = build_optional_value<value_data_ref>(annunciator.door_open());
    if(!master_warn || !eng_fire || !oil_low || !fuel_low || !anti_ice || !starter || !apu ||
       !master_caution || !vacuum_low || !hydro_low || !aux_fuel || !parking_brake ||
       !volt_low || !door_open) {
        return std::unexpected(0);
    }
    return annunciator_data_ref(
        std::move(master_warn.value()), std::move(eng_fire.value()), std::move(oil_low.value()),
        std::move(fuel_low.value()), std::move(anti_ice.value()), std::move(starter.value()),
        std::move(apu.value()), std::move(master_caution.value()), std::move(vacuum_low.value()),
        std::move(hydro_low.value()), std::move(aux_fuel.value()), std::move(parking_brake.value()),
        std::move(volt_low.value()), std::move(door_open.value())
    );
}

std::optional<bool> annunciator_data_ref::master_warn() const noexcept { return master_warn_.transform(&value_data_ref::is_set); }
std::optional<bool> annunciator_data_ref::eng_fire() const noexcept { return eng_fire_.transform(&value_data_ref::is_set); }
std::optional<bool> annunciator_data_ref::oil_low() const noexcept { return oil_low_.transform(&value_data_ref::is_set); }
std::optional<bool> annunciator_data_ref::fuel_low() const noexcept { return fuel_low_.transform(&value_data_ref::is_set); }
std::optional<bool> annunciator_data_ref::anti_ice() const noexcept { return anti_ice_.transform(&value_data_ref::is_set); }
std::optional<bool> annunciator_data_ref::starter() const noexcept { return starter_.transform(&value_data_ref::is_set); }
std::optional<bool> annunciator_data_ref::apu() const noexcept { return apu_.transform(&value_data_ref::is_set); }
std::optional<bool> annunciator_data_ref::master_caution() const noexcept { return master_caution_.transform(&value_data_ref::is_set); }
std::optional<bool> annunciator_data_ref::vacuum_low() const noexcept { return vacuum_low_.transform(&value_data_ref::is_set); }
std::optional<bool> annunciator_data_ref::hydro_low() const noexcept { return hydro_low_.transform(&value_data_ref::is_set); }
std::optional<bool> annunciator_data_ref::aux_fuel() const noexcept { return aux_fuel_.transform(&value_data_ref::is_set); }
std::optional<bool> annunciator_data_ref::parking_brake() const noexcept { return parking_brake_.transform(&value_data_ref::is_set); }
std::optional<bool> annunciator_data_ref::volt_low() const noexcept { return volt_low_.transform(&value_data_ref::is_set); }
std::optional<bool> annunciator_data_ref::door_open() const noexcept { return door_open_.transform(&value_data_ref::is_set); }

configuration::configuration(system_data_ref && system, std::optional<autopilot_data_ref> && autopilot,
                             std::optional<annunciator_data_ref> && annunciator) noexcept :
    system_(std::move(system)), autopilot_(std::move(autopilot)), annunciator_(std::move(annunciator))
{}

std::expected<configuration::ptr_type, int>
configuration::build(const conf::configuration & source)
{
    auto system = system_data_ref::build(source.system());
    if(!system) return std::unexpected(system.error());
    std::optional<autopilot_data_ref> autopilot;
    if(source.autopilot()) {
        auto bound = autopilot_data_ref::build(*source.autopilot());
        if(!bound) return std::unexpected(bound.error());
        autopilot.emplace(std::move(bound.value()));
    }
    std::optional<annunciator_data_ref> annunciator;
    if(source.annunciator()) {
        auto bound = annunciator_data_ref::build(*source.annunciator());
        if(!bound) return std::unexpected(bound.error());
        annunciator.emplace(std::move(bound.value()));
    }
    return ptr_type(new configuration(
        std::move(system.value()), std::move(autopilot), std::move(annunciator)
    ));
}

#if !defined(NDEBUG)
bool
value_data_ref::dump_state(const char * label) const noexcept
{
    bool result = false;
    // Dump every input, including entries skipped by the normal OR evaluation.
    for(const auto & data : data_) {
        const bool set = data->dump_state();
        result = result || set;
    }
    logger::debug() << "[State dump] " << label << "=" << result;
    return result;
}

void
float_data_ref::dump_state(const char * label) const noexcept
{
    const float value = get();
    logger::debug() << "[State dump] " << label << "=" << value << " DataRef='" << path_
        << "' index=" << (index_ ? std::to_string(*index_) : "scalar");
}

template<typename T>
static void dump_optional_state(const char * label, const std::optional<T> & value) noexcept
{
    if(value) value->dump_state(label);
    else logger::debug() << "[State dump] " << label << "=unconfigured";
}

void
airspeed_data_ref::dump_state() const noexcept
{
    const bool mach = is_mach_->dump_state();
    logger::debug() << "[State dump] autopilot.dials.ias.unit=" << (mach ? "Mach" : "Knots");
    value_.dump_state("autopilot.dials.ias.value");
}

void
autopilot_dial_data_ref::dump_state() const noexcept
{
    dump_optional_state("autopilot.dials.course", course_);
    dump_optional_state("autopilot.dials.heading", heading_);
    dump_optional_state("autopilot.dials.vs", vs_);
    dump_optional_state("autopilot.dials.alt", alt_);
    if(ias_) ias_->dump_state();
    else logger::debug() << "[State dump] autopilot.dials.ias=unconfigured";
}

void
autopilot_mode_data_ref::dump_state() const noexcept
{
    ap_.dump_state("autopilot.mode.ap");
    dump_optional_state("autopilot.mode.hdg", hdg_);
    dump_optional_state("autopilot.mode.nav", nav_);
    dump_optional_state("autopilot.mode.apr", apr_);
    dump_optional_state("autopilot.mode.rev", rev_);
    dump_optional_state("autopilot.mode.alt", alt_);
    dump_optional_state("autopilot.mode.vs", vs_);
    dump_optional_state("autopilot.mode.ias", ias_);
}

void
autopilot_data_ref::dump_state() const noexcept
{
    mode_.dump_state();
    if(dials_) dials_->dump_state();
    else logger::debug() << "[State dump] autopilot.dials=unconfigured";
}

gear_state
gear_signals_data_ref::dump_state(const char * label) const noexcept
{
    const bool safe = safe_.dump_state((std::string(label) + ".safe").c_str());
    const bool unsafe = unsafe_.dump_state((std::string(label) + ".unsafe").c_str());
    const auto result = safe ? gear_state::safe : unsafe ? gear_state::unsafe : gear_state::up;
    logger::debug() << "[State dump] " << label << "="
        << (safe ? "green" : unsafe ? "red" : "off") << " (safe takes priority over unsafe)";
    return result;
}

void
gear_data_ref::dump_state() const noexcept
{
    left_.dump_state("gear.left");
    nose_.dump_state("gear.nose");
    right_.dump_state("gear.right");
}

void
system_data_ref::dump_state() const noexcept
{
    const bool powered = volts_.dump_state("system.volts");
    logger::debug() << "[State dump] " << (powered ? "LED power enabled" : "All LEDs off: electrical power gate disabled");
    if(gear_) gear_->dump_state();
    else logger::debug() << "[State dump] gear=unconfigured";
}

void
annunciator_data_ref::dump_state() const noexcept
{
    dump_optional_state("annunciator.master_warn", master_warn_);
    dump_optional_state("annunciator.eng_fire", eng_fire_);
    dump_optional_state("annunciator.oil_low", oil_low_);
    dump_optional_state("annunciator.fuel_low", fuel_low_);
    dump_optional_state("annunciator.anti_ice", anti_ice_);
    dump_optional_state("annunciator.starter", starter_);
    dump_optional_state("annunciator.apu", apu_);
    dump_optional_state("annunciator.master_caution", master_caution_);
    dump_optional_state("annunciator.vacuum_low", vacuum_low_);
    dump_optional_state("annunciator.hydro_low", hydro_low_);
    dump_optional_state("annunciator.aux_fuel", aux_fuel_);
    dump_optional_state("annunciator.parking_brake", parking_brake_);
    dump_optional_state("annunciator.volt_low", volt_low_);
    dump_optional_state("annunciator.door_open", door_open_);
}
#endif

} // namespace profile
