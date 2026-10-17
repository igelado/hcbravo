// SPDX-License-Identifier: LGPL-2.1-only
// Exercise the runtime with aircraft DataRefs and HID reports supplied by test doubles.
#include <gtest/gtest.h>
#include <algorithm>

#include "state.h"
#include "logger.h"
#include <XPLM/XPLMPlugin.h>

int XPluginStart(char *, char *, char *);
void XPluginStop();
void XPluginDisable();
int XPluginEnable();
void XPluginReceiveMessage(XPLMPluginID, int, void *);

namespace runtime_test {

class runtime_test : public testing::Test {
protected:
    bool plugin_started_ = false;

    void SetUp() override {
        xplm_test::data_refs.clear();
        xplm_test::read_counts.clear();
        xplm_test::last_access.clear();
        xplm_test::strings = {
            {"sim/aircraft/view/acf_ICAO", "EVOT"},
            {"sim/aircraft/view/acf_ui_name", "Lancair Evolution"}
        };
        xplm_test::log.clear();
        xplm_test::schedule_count = 0;
        xplm_test::features.clear();
        xplm_test::menu_items.clear();
        xplm_test::menu_checks.clear();
        xplm_test::commands.clear();
#if !defined(NDEBUG)
        logger::diagnostics::reset();
#endif
        hid_test::reports.clear();
        hid_test::result = 65;
        for(const auto path : {
            "sim/cockpit2/autopilot/airspeed_dial_kts_mach",
            "sim/cockpit2/radios/actuators/nav1_obs_deg_mag_pilot",
            "sim/cockpit2/autopilot/heading_dial_deg_mag_pilot",
            "sim/cockpit2/autopilot/vvi_dial_fpm",
            "sim/cockpit2/autopilot/altitude_dial_ft",
            "laminar/n844x/lights/left_gear_down",
            "laminar/n844x/lights/nose_gear_down",
            "laminar/n844x/lights/right_gear_down",
            "laminar/n844x/lights/any_gear_in_transit"
        }) xplm_test::data_refs[path].type = xplmType_Float;
        xplm_test::data_refs["sim/cockpit2/electrical/bus_volts"] = {xplmType_FloatArray, true, 1, 28};
        xplm_test::data_refs["sim/cockpit2/controls/parking_brake_ratio"].type = xplmType_Float;
        xplm_test::data_refs["sim/flightmodel2/misc/door_open_ratio"].type = xplmType_FloatArray;
        // X-Plane publishes the per-engine pump under engine/actuators, not fuel.
        xplm_test::data_refs["sim/cockpit2/fuel/fuel_pump_on"].available = false;
        xplm_test::data_refs["sim/cockpit2/engine/actuators/fuel_pump_on"].type = xplmType_IntArray;
    }

    void TearDown() override {
        if(plugin_started_) XPluginStop();
    }

    bool start_plugin() {
        char name[256] {}, signature[256] {}, description[256] {};
        plugin_started_ = XPluginStart(name, signature, description) != 0;
        // X-Plane enables the plugin right after starting it.
        if(plugin_started_) {
            XPluginEnable();
            xplm_test::schedule_count = 0;
        }
        return plugin_started_;
    }

    void expect_debug_message(const char * message) {
#if !defined(NDEBUG)
        EXPECT_NE(xplm_test::log.find(message), std::string::npos) << message;
#else
        EXPECT_EQ(xplm_test::log.find(message), std::string::npos) << message;
#endif
    }
};

TEST_F(runtime_test, logger_retains_errors_and_gates_debug_messages_by_build_configuration) {
    logger::info() << "Runtime error example";
    logger::debug() << "Debug message example";
    EXPECT_NE(xplm_test::log.find("Runtime error example"), std::string::npos);
    expect_debug_message("Debug message example");
#if !defined(NDEBUG)
    {
        logger::diagnostic_scope snapshot(logger::diagnostic_category::datarefs);
        EXPECT_TRUE(logger::diagnostic_scope::enabled(logger::diagnostic_category::datarefs));
        EXPECT_FALSE(logger::diagnostic_scope::enabled(logger::diagnostic_category::configuration));
        {
            logger::diagnostic_scope nested(logger::diagnostic_category::configuration);
            EXPECT_TRUE(logger::diagnostic_scope::enabled(logger::diagnostic_category::configuration));
            EXPECT_FALSE(logger::diagnostic_scope::enabled(logger::diagnostic_category::datarefs));
        }
        EXPECT_TRUE(logger::diagnostic_scope::enabled(logger::diagnostic_category::datarefs));
    }
    EXPECT_FALSE(logger::diagnostic_scope::enabled(logger::diagnostic_category::datarefs));
#endif
}

TEST_F(runtime_test, logs_plugin_and_aircraft_lifecycle_without_per_frame_messages) {
    ASSERT_TRUE(start_plugin());
    EXPECT_NE(xplm_test::log.find("Plugin loaded"), std::string::npos);
    XPluginReceiveMessage(XPLM_PLUGIN_XPLANE, XPLM_MSG_PLANE_LOADED, nullptr);
    ASSERT_EQ(xplm_test::tick(), state::state::powered_interval);
    EXPECT_NE(xplm_test::log.find("Aircraft enabled: 'Lancair Evolution' (EVOT) using profile 'Lancair Evolution'"), std::string::npos);

    xplm_test::log.clear();
    EXPECT_EQ(xplm_test::tick(), state::state::powered_interval);
    EXPECT_EQ(xplm_test::tick(), state::state::powered_interval);
    EXPECT_TRUE(xplm_test::log.empty());
    XPluginStop();
    plugin_started_ = false;
    const auto disabled = xplm_test::log.find("Aircraft disabled: 'Lancair Evolution'");
    const auto unloaded = xplm_test::log.find("Plugin unloaded");
    EXPECT_NE(disabled, std::string::npos);
    EXPECT_NE(unloaded, std::string::npos);
    EXPECT_LT(disabled, unloaded);
}

TEST_F(runtime_test, defers_binding_and_dumps_raw_values_gear_decisions_and_cached_hid_output) {
    xplm_test::data_refs["laminar/n844x/lights/left_gear_down"].value = 1;
    xplm_test::data_refs["laminar/n844x/lights/any_gear_in_transit"].value = 1;
    xplm_test::data_refs["sim/cockpit2/engine/actuators/fuel_pump_on"].value = 1;
    auto initialized = state::state::init();
    ASSERT_TRUE(initialized.has_value());
    auto & runtime = *initialized.value();
#if !defined(NDEBUG)
    for(size_t id : {5, 6}) xplm_test::menu_handler(xplm_test::menu_refcon, reinterpret_cast<void *>(id));
    runtime.reload();
#endif
    runtime.request_load_plane();
    EXPECT_FALSE(runtime.active_plane().has_value());
    EXPECT_EQ(xplm_test::schedule_count, 1);
    EXPECT_EQ(xplm_test::scheduled_interval, -1);
    EXPECT_EQ(xplm_test::log.find("DataRef bound:"), std::string::npos);

    EXPECT_EQ(xplm_test::tick(), state::state::powered_interval);
    EXPECT_TRUE(runtime.active_plane().has_value());
    EXPECT_EQ(xplm_test::log.find("[DataRef debug]"), std::string::npos);
#if !defined(NDEBUG)
    xplm_test::menu_handler(xplm_test::menu_refcon, reinterpret_cast<void *>(2));
#endif
    expect_debug_message("raw=28 count=1");
    expect_debug_message("gear.left=green");
    expect_debug_message("gear.nose=red");
    expect_debug_message("gear.right=red");
    expect_debug_message("HID available=1 synchronized=1");
    expect_debug_message("cached banks=[00 29 04 01]");
    ASSERT_EQ(hid_test::reports.size(), 2);
    EXPECT_EQ(hid_test::reports.back()[2] & 0x3f, 0x29); // left green, nose/right red
    EXPECT_EQ(hid_test::reports.back()[4] & 0x01, 0x01); // auxiliary fuel pump
    expect_debug_message("DataRef 'sim/cockpit2/engine/actuators/fuel_pump_on' index=0 reader=int raw=1 count=1");
    expect_debug_message("DataRef bound:");
    expect_debug_message("Loading YAML File");
    expect_debug_message("Checking for required");
}

TEST_F(runtime_test, evolution_uses_parking_brake_and_float_door_inputs_for_annunciators) {
    xplm_test::data_refs["sim/cockpit2/controls/parking_brake_ratio"].value = 0.5;
    xplm_test::data_refs["sim/flightmodel2/misc/door_open_ratio"].value = 0.25;
    auto initialized = state::state::init();
    ASSERT_TRUE(initialized.has_value());
    initialized.value()->request_load_plane();
    ASSERT_EQ(xplm_test::tick(), state::state::powered_interval);
    ASSERT_TRUE(initialized.value()->active_plane().has_value());
    ASSERT_FALSE(hid_test::reports.empty());
    // Parking brake and door are bits 1 and 3 in the fourth bank.
    EXPECT_EQ(hid_test::reports.back()[4] & 0x0a, 0x0a);
}

TEST_F(runtime_test, loads_again_only_when_datarefs_are_added_after_a_failed_load) {
    auto & gear = xplm_test::data_refs["laminar/n844x/lights/left_gear_down"];
    gear.available = false;
    auto initialized = state::state::init();
    ASSERT_TRUE(initialized.has_value());
    initialized.value()->request_load_plane();
    EXPECT_EQ(xplm_test::tick(), 0);
    EXPECT_FALSE(initialized.value()->active_plane().has_value());
    gear.available = true;
    xplm_test::log.clear();
    EXPECT_EQ(xplm_test::tick(), 0);
    EXPECT_FALSE(initialized.value()->active_plane().has_value());
    EXPECT_EQ(xplm_test::log.find("DataRef bound:"), std::string::npos);
    EXPECT_EQ(xplm_test::schedule_count, 1);
    initialized.value()->data_refs_added();
    EXPECT_EQ(xplm_test::scheduled_interval, -1);
    EXPECT_FALSE(initialized.value()->active_plane().has_value());
    EXPECT_EQ(xplm_test::tick(), state::state::powered_interval);
    EXPECT_TRUE(initialized.value()->active_plane().has_value());
}

TEST_F(runtime_test, failed_binding_stops_without_scheduling_a_retry) {
    xplm_test::data_refs["laminar/n844x/lights/left_gear_down"].available = false;
    auto initialized = state::state::init();
    ASSERT_TRUE(initialized.has_value());
    initialized.value()->request_load_plane();
    EXPECT_EQ(xplm_test::tick(), 0);
    EXPECT_FALSE(initialized.value()->active_plane().has_value());
    EXPECT_EQ(xplm_test::schedule_count, 1);
    EXPECT_NE(xplm_test::log.find("Profile binding failed; LEDs inactive"), std::string::npos);
}

TEST_F(runtime_test, dataref_notifications_coalesce_and_allow_loading_after_another_failure) {
    auto & gear = xplm_test::data_refs["laminar/n844x/lights/left_gear_down"];
    gear.available = false;
    auto initialized = state::state::init();
    ASSERT_TRUE(initialized.has_value());
    auto & runtime = *initialized.value();
    runtime.request_load_plane();
    EXPECT_EQ(xplm_test::tick(), 0);

    // Repeated notifications coalesce into a single deferred attempt.
    runtime.data_refs_added();
    runtime.data_refs_added();
    EXPECT_EQ(xplm_test::schedule_count, 2);
    EXPECT_EQ(xplm_test::scheduled_interval, -1);
    EXPECT_EQ(xplm_test::tick(), 0); // An unrelated new DataRef did not fix the dependency.
    gear.available = true;
    runtime.data_refs_added();
    EXPECT_EQ(xplm_test::tick(), state::state::powered_interval);
    EXPECT_TRUE(runtime.active_plane().has_value());

    const auto schedule_count = xplm_test::schedule_count;
    runtime.data_refs_added();
    EXPECT_EQ(xplm_test::schedule_count, schedule_count);
}

TEST_F(runtime_test, flight_loop_polls_at_a_slow_interval_instead_of_every_frame) {
    static_assert(state::state::powered_interval > 0.0f);
    static_assert(state::state::unpowered_interval >= state::state::powered_interval);
    ASSERT_TRUE(start_plugin());
    XPluginReceiveMessage(XPLM_PLUGIN_XPLANE, XPLM_MSG_PLANE_LOADED, nullptr);
    EXPECT_EQ(xplm_test::tick(), state::state::powered_interval);
}

TEST_F(runtime_test, disabling_the_plugin_darkens_leds_and_stops_the_flight_loop) {
    xplm_test::data_refs["laminar/n844x/lights/left_gear_down"].value = 1;
    ASSERT_TRUE(start_plugin());
    XPluginReceiveMessage(XPLM_PLUGIN_XPLANE, XPLM_MSG_PLANE_LOADED, nullptr);
    EXPECT_EQ(xplm_test::tick(), state::state::powered_interval);
    ASSERT_FALSE(hid_test::reports.empty());
    const auto & lit = hid_test::reports.back();
    EXPECT_TRUE(std::any_of(lit.begin() + 1, lit.end(), [](auto byte) { return byte != 0; }));

    XPluginDisable();
    EXPECT_EQ(xplm_test::scheduled_interval, 0);
    const auto & dark = hid_test::reports.back();
    EXPECT_TRUE(std::all_of(dark.begin() + 1, dark.end(), [](auto byte) { return byte == 0; }));
    EXPECT_EQ(xplm_test::tick(), 0);

    // Nothing rebinds the aircraft while disabled.
    const int scheduled = xplm_test::schedule_count;
    XPluginReceiveMessage(XPLM_PLUGIN_XPLANE, XPLM_MSG_PLANE_LOADED, nullptr);
    EXPECT_EQ(xplm_test::schedule_count, scheduled);
    EXPECT_EQ(xplm_test::tick(), 0);

    // Re-enabling rebinds the current aircraft and the LEDs light again.
    EXPECT_EQ(XPluginEnable(), 1);
    EXPECT_EQ(xplm_test::schedule_count, scheduled + 1);
    EXPECT_EQ(xplm_test::scheduled_interval, -1);
    EXPECT_EQ(xplm_test::tick(), state::state::powered_interval);
    const auto & relit = hid_test::reports.back();
    EXPECT_TRUE(std::any_of(relit.begin() + 1, relit.end(), [](auto byte) { return byte != 0; }));
}

TEST_F(runtime_test, first_enable_binds_an_aircraft_that_was_already_loaded) {
    char name[256] {}, signature[256] {}, description[256] {};
    plugin_started_ = XPluginStart(name, signature, description) != 0;
    ASSERT_TRUE(plugin_started_);
    // The plugin starts disabled; X-Plane will not replay PLANE_LOADED for it.
    XPluginReceiveMessage(XPLM_PLUGIN_XPLANE, XPLM_MSG_PLANE_LOADED, nullptr);
    EXPECT_EQ(xplm_test::schedule_count, 0);
    EXPECT_EQ(XPluginEnable(), 1);
    EXPECT_EQ(xplm_test::schedule_count, 1);
    EXPECT_EQ(xplm_test::tick(), -1);
    EXPECT_NE(xplm_test::log.find("Aircraft enabled: 'Lancair Evolution'"), std::string::npos);
}

TEST_F(runtime_test, enabling_an_already_enabled_plugin_does_not_reload_the_aircraft) {
    ASSERT_TRUE(start_plugin());
    EXPECT_EQ(XPluginEnable(), 1);
    EXPECT_EQ(xplm_test::schedule_count, 0);
    XPluginDisable();
    XPluginDisable();
    EXPECT_EQ(xplm_test::schedule_count, 1);
}

TEST_F(runtime_test, unloading_cancels_pending_binding) {
    auto initialized = state::state::init();
    ASSERT_TRUE(initialized.has_value());
    initialized.value()->request_load_plane();
    initialized.value()->unload_plane();
    initialized.value()->data_refs_added();
    EXPECT_EQ(xplm_test::schedule_count, 1);
    EXPECT_EQ(xplm_test::tick(), 0);
    EXPECT_FALSE(initialized.value()->active_plane().has_value());
    EXPECT_EQ(xplm_test::log.find("DataRef bound:"), std::string::npos);
}

TEST_F(runtime_test, simulator_events_enable_notifications_and_defer_aircraft_binding) {
    ASSERT_TRUE(start_plugin());
    EXPECT_EQ(xplm_test::features["XPLM_WANTS_DATAREF_NOTIFICATIONS"], 1);
    XPluginReceiveMessage(123, XPLM_MSG_PLANE_LOADED, nullptr); // Not the simulator.
    XPluginReceiveMessage(XPLM_PLUGIN_XPLANE, XPLM_MSG_PLANE_LOADED, reinterpret_cast<void *>(1)); // AI aircraft.
    EXPECT_EQ(xplm_test::schedule_count, 0);
    XPluginReceiveMessage(XPLM_PLUGIN_XPLANE, XPLM_MSG_PLANE_LOADED, nullptr);
    EXPECT_EQ(xplm_test::schedule_count, 1);
    EXPECT_EQ(xplm_test::log.find("DataRef bound:"), std::string::npos);
    EXPECT_EQ(xplm_test::tick(), state::state::powered_interval);
    EXPECT_NE(xplm_test::log.find("Aircraft enabled: 'Lancair Evolution' (EVOT) using profile 'Lancair Evolution'"), std::string::npos);
}

TEST_F(runtime_test, simulator_dataref_notification_wakes_failed_binding_but_not_unloaded_aircraft) {
    auto & gear = xplm_test::data_refs["laminar/n844x/lights/left_gear_down"];
    gear.available = false;
    ASSERT_TRUE(start_plugin());
    XPluginReceiveMessage(XPLM_PLUGIN_XPLANE, XPLM_MSG_PLANE_LOADED, nullptr);
    EXPECT_EQ(xplm_test::tick(), 0);
    gear.available = true;
    int dataref_count = 2000;
    XPluginReceiveMessage(XPLM_PLUGIN_XPLANE, XPLM_MSG_DATAREFS_ADDED, &dataref_count);
    EXPECT_EQ(xplm_test::schedule_count, 2);
    EXPECT_EQ(xplm_test::tick(), state::state::powered_interval);
    XPluginReceiveMessage(XPLM_PLUGIN_XPLANE, XPLM_MSG_PLANE_UNLOADED, nullptr);
    EXPECT_NE(xplm_test::log.find("Aircraft disabled: 'Lancair Evolution'"), std::string::npos);
    xplm_test::log.clear();
    XPluginReceiveMessage(XPLM_PLUGIN_XPLANE, XPLM_MSG_PLANE_UNLOADED, nullptr);
    EXPECT_EQ(xplm_test::log.find("Aircraft disabled:"), std::string::npos);
    XPluginReceiveMessage(XPLM_PLUGIN_XPLANE, XPLM_MSG_DATAREFS_ADDED, &dataref_count);
    EXPECT_EQ(xplm_test::schedule_count, 2);
    EXPECT_EQ(xplm_test::tick(), 0);
}

TEST_F(runtime_test, state_dump_menu_actions_are_immediate_repeatable_and_do_not_write_hid) {
    auto initialized = state::state::init();
    ASSERT_TRUE(initialized.has_value());
    std::vector<std::string> expected_menu_items {"HoneyComb Bravo", "Reload Aircraft Profiles"};
#if !defined(NDEBUG)
    expected_menu_items.emplace_back("Reload All Plugins");
    expected_menu_items.emplace_back("Dump LED State");
    expected_menu_items.emplace_back("Dump Autopilot State");
    expected_menu_items.emplace_back("Knob Diagnostics");
    expected_menu_items.emplace_back("DataRef Diagnostics");
    expected_menu_items.emplace_back("Configuration Diagnostics");
#endif
    EXPECT_EQ(xplm_test::menu_items, expected_menu_items);
    initialized.value()->request_load_plane();
    ASSERT_EQ(xplm_test::tick(), state::state::powered_interval);
    xplm_test::log.clear();
    EXPECT_EQ(xplm_test::tick(), state::state::powered_interval);
    EXPECT_TRUE(xplm_test::log.empty());
    const auto reports = hid_test::reports.size();
    const auto schedules = xplm_test::schedule_count;
#if !defined(NDEBUG)
    for(size_t id : {2, 3, 2, 3}) {
        xplm_test::menu_handler(xplm_test::menu_refcon, reinterpret_cast<void *>(id));
        expect_debug_message(id == 2 ? "[LED dump]" : "[Autopilot dump]");
        EXPECT_EQ(logger::diagnostics::selected(), 0);
        EXPECT_EQ(xplm_test::log.find(id == 2 ? "[Autopilot dump]" : "[LED dump]"), std::string::npos);
        xplm_test::log.clear();
        EXPECT_EQ(xplm_test::tick(), state::state::powered_interval);
        EXPECT_TRUE(xplm_test::log.empty());
    }
    // Dump actions have no checkboxes. The mock includes the parent menu item.
    EXPECT_FALSE(xplm_test::menu_checks.contains(3));
    EXPECT_FALSE(xplm_test::menu_checks.contains(4));
#else
    EXPECT_TRUE(xplm_test::menu_checks.empty());
#endif
    EXPECT_EQ(hid_test::reports.size(), reports);
    EXPECT_EQ(xplm_test::schedule_count, schedules);
}

TEST_F(runtime_test, autopilot_dump_reads_current_dials_and_modes_even_without_led_power) {
    xplm_test::data_refs["sim/cockpit2/electrical/bus_volts"].value = 0;
    xplm_test::data_refs["sim/cockpit2/autopilot/servos_on"].value = 1;
    xplm_test::data_refs["sim/cockpit2/autopilot/heading_mode"].value = 14;
    xplm_test::data_refs["sim/cockpit2/autopilot/airspeed_is_mach"].value = 1;
    auto initialized = state::state::init();
    ASSERT_TRUE(initialized.has_value());
    initialized.value()->request_load_plane();
    ASSERT_EQ(xplm_test::tick(), state::state::unpowered_interval);
    const auto & heading = initialized.value()->active_plane().value()->autopilot()->dials()->heading().value();
    const auto reports = hid_test::reports.size();
    for(float value : {123.0f, 247.0f}) {
        heading.set(value);
        xplm_test::log.clear();
#if !defined(NDEBUG)
        xplm_test::menu_handler(xplm_test::menu_refcon, reinterpret_cast<void *>(3));
        EXPECT_NE(xplm_test::log.find("autopilot.dials.heading=" + std::to_string(static_cast<int>(value))), std::string::npos);
#endif
        expect_debug_message("LED power enabled=0");
        expect_debug_message("autopilot.mode.ap=1");
        expect_debug_message("autopilot.mode.hdg=1");
        expect_debug_message("autopilot.dials.ias.unit=Mach");
        EXPECT_FLOAT_EQ(heading.get(), value);
    }
    EXPECT_EQ(hid_test::reports.size(), reports);
}

TEST_F(runtime_test, dumps_without_a_profile_do_not_load_retry_or_read_stale_datarefs) {
    auto initialized = state::state::init();
    ASSERT_TRUE(initialized.has_value());
    auto & runtime = *initialized.value();
    // Exercise initial state, a pending load, a failed load, and an unloaded aircraft.
    for(int phase = 0; phase < 4; ++phase) {
        if(phase == 1) runtime.request_load_plane();
        if(phase == 2) {
            xplm_test::data_refs["laminar/n844x/lights/left_gear_down"].available = false;
            EXPECT_EQ(xplm_test::tick(), 0);
        }
        if(phase == 3) runtime.unload_plane();
        const auto schedules = xplm_test::schedule_count;
        const auto reports = hid_test::reports.size();
        xplm_test::log.clear();
        xplm_test::read_counts.clear();
#if !defined(NDEBUG)
        for(size_t id : {2, 3}) xplm_test::menu_handler(xplm_test::menu_refcon, reinterpret_cast<void *>(id));
#endif
        expect_debug_message("[LED dump] No active aircraft profile");
        expect_debug_message("[Autopilot dump] No active aircraft profile");
        EXPECT_FALSE(runtime.active_plane());
        EXPECT_EQ(xplm_test::schedule_count, schedules);
        EXPECT_EQ(hid_test::reports.size(), reports);
        EXPECT_TRUE(xplm_test::read_counts.empty());
    }
}

TEST_F(runtime_test, power_gate_keeps_all_leds_off_and_dump_reports_array_read_failures) {
    xplm_test::data_refs["sim/cockpit2/electrical/bus_volts"].array_read_count = 0;
    auto initialized = state::state::init();
    ASSERT_TRUE(initialized.has_value());
#if !defined(NDEBUG)
    xplm_test::menu_handler(xplm_test::menu_refcon, reinterpret_cast<void *>(5));
#endif
    initialized.value()->request_load_plane();
    EXPECT_EQ(xplm_test::tick(), state::state::unpowered_interval);
#if !defined(NDEBUG)
    xplm_test::menu_handler(xplm_test::menu_refcon, reinterpret_cast<void *>(2));
#endif
    expect_debug_message("READ FAILED");
    expect_debug_message("system.volts=0");
    expect_debug_message("All LEDs off: electrical power gate disabled");
    ASSERT_EQ(hid_test::reports.size(), 1);
    for(const auto byte : hid_test::reports[0]) EXPECT_EQ(byte, 0);
}

class test_led_state : public led::led_state {
public:
    test_led_state() { hid_ = &hid_test::device; }
    void allow_retry() { next_retry_ = {}; }
};

TEST_F(runtime_test, failed_hid_write_is_logged_throttled_and_retried) {
    test_led_state leds;
    led::led_mask mask;
    mask.update(led::LED_LDG_L_GREEN, true);
    hid_test::result = -1;
    leds.update(mask);
    EXPECT_NE(xplm_test::log.find("bytes=-1/65 error=test HID failure"), std::string::npos);
    hid_test::result = 65;
    leds.update(mask);
    EXPECT_EQ(hid_test::reports.size(), 1);
    leds.allow_retry();
    leds.update(mask);
    EXPECT_EQ(hid_test::reports.size(), 2);
    leds.update(mask);
    EXPECT_EQ(hid_test::reports.size(), 2);
}

TEST_F(runtime_test, short_hid_write_does_not_mark_output_as_synchronized) {
    test_led_state leds;
    led::led_mask mask;
    hid_test::result = 4;
    leds.update(mask);
    EXPECT_NE(xplm_test::log.find("bytes=4/65"), std::string::npos);
    hid_test::result = 65;
    leds.allow_retry();
    leds.update(mask);
    EXPECT_EQ(hid_test::reports.size(), 2);
}

TEST_F(runtime_test, turning_off_leds_bypasses_cooldown_after_a_failed_nonzero_write) {
    test_led_state leds;
    led::led_mask green;
    green.update(led::LED_LDG_L_GREEN, true);
    leds.update(green);
    led::led_mask red;
    red.update(led::LED_LDG_L_RED, true);
    hid_test::result = -1;
    leds.update(red);
    ASSERT_EQ(hid_test::reports.size(), 2);
    hid_test::result = 65;
    leds.update(red);
    EXPECT_EQ(hid_test::reports.size(), 2); // Regular updates still respect cooldown.
    leds.turn_off();
    ASSERT_EQ(hid_test::reports.size(), 3);
    for(const auto byte : hid_test::reports.back()) EXPECT_EQ(byte, 0);
    leds.turn_off();
    EXPECT_EQ(hid_test::reports.size(), 3); // Already synchronized to all-off.
}

TEST_F(runtime_test, plugin_stop_attempts_all_off_even_when_led_writes_are_in_cooldown) {
    ASSERT_TRUE(start_plugin());
    XPluginReceiveMessage(XPLM_PLUGIN_XPLANE, XPLM_MSG_PLANE_LOADED, nullptr);
    hid_test::result = -1;
    ASSERT_EQ(xplm_test::tick(), state::state::powered_interval);
    const auto reports = hid_test::reports.size();
    hid_test::result = 65;
    XPluginStop();
    plugin_started_ = false;
    ASSERT_EQ(hid_test::reports.size(), reports + 1);
    for(const auto byte : hid_test::reports.back()) EXPECT_EQ(byte, 0);
}

TEST_F(runtime_test, logs_raw_values_and_applies_value_filters_before_inversion) {
#if !defined(NDEBUG)
    logger::diagnostics::toggle(logger::diagnostic_category::datarefs);
#endif
    xplm_test::data_refs["sim/test/mode"].value = 2;
    auto key = conf::key::build(YAML::Load("{key: sim/test/mode, values: [1, 14], invert: true}"));
    ASSERT_TRUE(key.has_value());
    auto value = profile::bool_data_ref::build(key.value());
    ASSERT_TRUE(value.has_value());
#if !defined(NDEBUG)
    EXPECT_TRUE(value.value()->dump_state());
#else
    EXPECT_TRUE(value.value()->is_set());
#endif
    expect_debug_message("values=[1 14 ]");
    expect_debug_message("raw=2 count=1 invert=1 result=1");
    xplm_test::log.clear();
    EXPECT_TRUE(value.value()->is_set());
    EXPECT_TRUE(xplm_test::log.empty());
}

TEST_F(runtime_test, failed_indexed_read_is_never_set_even_when_inverted) {
    xplm_test::data_refs["sim/test/array"] = {xplmType_FloatArray, true, 1, 0};
    xplm_test::data_refs["sim/test/array"].array_read_count = 0;
    for(const char * node : {
        "{key: sim/test/array, index: 0, invert: true}",
        "{key: sim/test/array, index: 0}"
    }) {
        auto key = conf::key::build(YAML::Load(node));
        ASSERT_TRUE(key.has_value());
        auto value = profile::bool_data_ref::build(key.value());
        ASSERT_TRUE(value.has_value());
        EXPECT_FALSE(value.value()->is_set()) << node;
    }
}

TEST_F(runtime_test, explicit_dump_reads_all_inputs_and_normal_updates_keep_short_circuiting) {
    xplm_test::data_refs["sim/test/first"].value = 1;
    auto source = conf::value::build(YAML::Load("[sim/test/first, sim/test/second]"));
    ASSERT_TRUE(source.has_value());
    auto value = profile::value_data_ref::build(source.value());
    ASSERT_TRUE(value.has_value());
    xplm_test::log.clear();
    EXPECT_TRUE(value->is_set());
    EXPECT_EQ(xplm_test::read_counts["sim/test/first"], 1);
    EXPECT_EQ(xplm_test::read_counts["sim/test/second"], 0);
    EXPECT_TRUE(xplm_test::log.empty());
    xplm_test::read_counts.clear();
#if !defined(NDEBUG)
    EXPECT_TRUE(value->dump_state("test.value"));
    EXPECT_EQ(xplm_test::read_counts["sim/test/first"], 1);
    EXPECT_EQ(xplm_test::read_counts["sim/test/second"], 1);
#endif
    expect_debug_message("DataRef 'sim/test/second'");
}

TEST_F(runtime_test, gear_dump_reads_unsafe_input_while_normal_updates_prioritize_safe) {
    xplm_test::data_refs["sim/test/safe"].value = 1;
    xplm_test::data_refs["sim/test/unsafe"].value = 1;
    auto source = conf::gear_signals::build(YAML::Load("{safe: sim/test/safe, unsafe: sim/test/unsafe}"));
    ASSERT_TRUE(source.has_value());
    auto signals = profile::gear_signals_data_ref::build(source.value());
    ASSERT_TRUE(signals.has_value());
    EXPECT_EQ(signals->state(), profile::gear_state::safe);
    EXPECT_EQ(xplm_test::read_counts["sim/test/safe"], 1);
    EXPECT_EQ(xplm_test::read_counts["sim/test/unsafe"], 0);
    xplm_test::read_counts.clear();
#if !defined(NDEBUG)
    EXPECT_EQ(signals->dump_state("gear.left"), profile::gear_state::safe);
    EXPECT_EQ(xplm_test::read_counts["sim/test/safe"], 1);
    EXPECT_EQ(xplm_test::read_counts["sim/test/unsafe"], 1);
#endif
    expect_debug_message("gear.left.safe=1");
    expect_debug_message("gear.left.unsafe=1");
    expect_debug_message("gear.left=green");
}

TEST_F(runtime_test, diagnostic_toggles_do_not_produce_flight_loop_output) {
    auto initialized = state::state::init();
    ASSERT_TRUE(initialized.has_value());
    initialized.value()->request_load_plane();
    ASSERT_EQ(xplm_test::tick(), state::state::powered_interval);
#if !defined(NDEBUG)
    EXPECT_EQ(logger::diagnostics::selected(), 0);
    for(const auto & entry : xplm_test::menu_checks) EXPECT_EQ(entry.second, xplm_Menu_Unchecked);
    for(size_t id : {4, 5, 6}) {
        xplm_test::menu_handler(xplm_test::menu_refcon, reinterpret_cast<void *>(id));
        // The mock records the parent item before the submenu's items.
        EXPECT_EQ(xplm_test::menu_checks.at(static_cast<int>(id + 1)), xplm_Menu_Checked);
    }
    xplm_test::log.clear();
    EXPECT_EQ(xplm_test::tick(), state::state::powered_interval);
    EXPECT_EQ(xplm_test::tick(), state::state::powered_interval);
    EXPECT_TRUE(xplm_test::log.empty());
    for(size_t id : {4, 5, 6}) {
        xplm_test::menu_handler(xplm_test::menu_refcon, reinterpret_cast<void *>(id));
        EXPECT_EQ(xplm_test::menu_checks.at(static_cast<int>(id + 1)), xplm_Menu_Unchecked);
        xplm_test::log.clear();
        EXPECT_EQ(xplm_test::tick(), state::state::powered_interval);
        EXPECT_TRUE(xplm_test::log.empty());
    }
#else
    EXPECT_TRUE(xplm_test::menu_checks.empty());
#endif
}

TEST_F(runtime_test, knob_diagnostics_are_event_driven_and_do_not_enable_other_categories) {
    auto initialized = state::state::init();
    ASSERT_TRUE(initialized.has_value());
    initialized.value()->request_load_plane();
    ASSERT_EQ(xplm_test::tick(), state::state::powered_interval);
    xplm_test::log.clear();
    xplm_test::invoke("HCBravo/HDG", xplm_CommandBegin);
    xplm_test::invoke("HCBravo/Inc", xplm_CommandEnd);
    EXPECT_TRUE(xplm_test::log.empty());
    const auto & heading = initialized.value()->active_plane().value()->autopilot()->dials()->heading().value();
    const float before = heading.get();
#if !defined(NDEBUG)
    xplm_test::menu_handler(xplm_test::menu_refcon, reinterpret_cast<void *>(4));
#endif
    xplm_test::log.clear();
    xplm_test::invoke("HCBravo/HDG", xplm_CommandBegin);
    xplm_test::invoke("HCBravo/Inc", xplm_CommandEnd);
    EXPECT_GT(heading.get(), before);
    expect_debug_message("[Knob debug] Selected HDG");
    expect_debug_message("[Knob debug] Adjusted HDG direction=increase");
    EXPECT_EQ(xplm_test::log.find("[DataRef debug]"), std::string::npos);
    EXPECT_EQ(xplm_test::log.find("[Autopilot dump]"), std::string::npos);
#if !defined(NDEBUG)
    xplm_test::menu_handler(xplm_test::menu_refcon, reinterpret_cast<void *>(4));
    xplm_test::menu_handler(xplm_test::menu_refcon, reinterpret_cast<void *>(5));
#endif
    xplm_test::log.clear();
    xplm_test::invoke("HCBravo/Inc", xplm_CommandEnd);
    expect_debug_message("[DataRef debug] Float read");
    expect_debug_message("[DataRef debug] Float write");
    EXPECT_EQ(xplm_test::log.find("[Knob debug]"), std::string::npos);
}

TEST_F(runtime_test, configuration_diagnostics_are_opt_in_and_independent_from_datarefs) {
    auto initialized = state::state::init();
    ASSERT_TRUE(initialized.has_value());
    EXPECT_EQ(xplm_test::log.find("[Configuration debug]"), std::string::npos);
#if !defined(NDEBUG)
    xplm_test::menu_handler(xplm_test::menu_refcon, reinterpret_cast<void *>(6));
#endif
    xplm_test::log.clear();
    initialized.value()->reload();
    expect_debug_message("[Configuration debug] Loading YAML File");
    EXPECT_EQ(xplm_test::log.find("DataRef bound:"), std::string::npos);
    initialized.value()->request_load_plane();
    EXPECT_EQ(xplm_test::tick(), state::state::powered_interval);
    EXPECT_EQ(xplm_test::log.find("DataRef bound:"), std::string::npos);
}

} // namespace runtime_test
