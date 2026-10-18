// SPDX-License-Identifier: LGPL-2.1-only
//
// Tests for runtime profiles built from parsed configurations.

#include <gtest/gtest.h>

#define HCBRAVO_PROFILE_TESTS
#include <conf.h>
#include <profile.h>
#include <XPLM/XPLMUtilities.h>

#include <filesystem>
#include <fstream>

namespace profile_test {

static std::expected<conf::configuration, int>
read_configuration(const std::string & contents)
{
    const auto path = std::filesystem::temp_directory_path() / "hcbravo-profile-test.yaml";
    {
        std::ofstream file(path);
        if(!file) return std::unexpected(0);
        file << contents;
    }

    auto configuration = conf::configuration::read(path.string());
    std::filesystem::remove(path);
    return configuration;
}

TEST(profile_test, builds_runtime_profile_from_configuration)
{
    auto source = read_configuration(R"(
name: Test Aircraft
aircrafts: [Test Aircraft]
models: [TEST]
system:
  volts: sim/test/volts
)");
    ASSERT_TRUE(source.has_value());

    auto runtime = profile::configuration::build(source.value());

    ASSERT_TRUE(runtime.has_value());
    EXPECT_FALSE(runtime.value()->system().volts());
    EXPECT_FALSE(runtime.value()->autopilot().has_value());
    EXPECT_FALSE(runtime.value()->annunciator().has_value());
}

TEST(profile_test, gear_state_is_derived_from_safe_and_unsafe_configuration)
{
    auto source = read_configuration(R"(
name: Test Gear Aircraft
aircrafts: [Test Gear Aircraft]
models: [TGEAR]
system:
  volts: sim/test/volts
  gear:
    left:
      safe: sim/test/left-safe
      unsafe: sim/test/any-gear-moving
    nose:
      safe: sim/test/nose-safe
      unsafe: sim/test/any-gear-moving
    right:
      safe: sim/test/right-safe
      unsafe: sim/test/any-gear-moving
)");
    ASSERT_TRUE(source.has_value());

    auto runtime = profile::configuration::build(source.value());

    ASSERT_TRUE(runtime.has_value());
    ASSERT_TRUE(runtime.value()->system().gear().has_value());
    const auto & gear = runtime.value()->system().gear().value();
    EXPECT_EQ(gear.left_state(), profile::gear_state::up);
    EXPECT_EQ(gear.nose_state(), profile::gear_state::up);
    EXPECT_EQ(gear.right_state(), profile::gear_state::up);
}

TEST(profile_test, rejects_values_incompatible_with_discovered_dataref_type)
{
    auto source = read_configuration(R"(
name: Invalid Value Aircraft
aircrafts: [Invalid Value Aircraft]
models: [IVAL]
system:
  volts:
    key: sim/test/volts
    values: [not-an-integer]
)");
    ASSERT_TRUE(source.has_value());

    EXPECT_FALSE(profile::configuration::build(source.value()).has_value());
}

TEST(profile_test, binds_zibo_profile_and_resolves_gear_up_transit_and_down)
{
    const auto source = conf::configuration::read(std::string(HCBRAVO_SOURCE_DIR) + "/conf/b738.yaml");
    ASSERT_TRUE(source.has_value());
    ASSERT_TRUE(source->system().gear().has_value());
    const auto & gear = source->system().gear().value();
    EXPECT_EQ(gear.left().safe().keys().front().index(), 1);
    EXPECT_EQ(gear.nose().safe().keys().front().index(), 0);
    EXPECT_EQ(gear.right().safe().keys().front().index(), 2);

    xplm_test::data_refs.clear();
    for(const auto path : {
        "laminar/B738/autopilot/mcp_alt_dial",
        "laminar/B738/autopilot/mcp_hdg_dial",
        "laminar/B738/autopilot/course_pilot",
        "laminar/B738/autopilot/mcp_speed_dial_kts_mach",
        "sim/cockpit2/autopilot/vvi_dial_fpm"
    }) xplm_test::data_refs[path].type = xplmType_Float;
    xplm_test::data_refs["laminar/B738/dc_volt_value"] = {xplmType_Float, true, 1, 28};
    xplm_test::data_refs["laminar/B738/autopilot/cmd_a_status"] = {xplmType_Float, true, 1, 1};
    const std::pair<double, profile::gear_state> cases[] = {
        {0, profile::gear_state::up}, {0.5, profile::gear_state::unsafe}, {1, profile::gear_state::safe}
    };
    for(const auto & [ratio, expected] : cases) {
        SCOPED_TRACE(ratio);
        xplm_test::data_refs["sim/flightmodel2/gear/deploy_ratio"] = {xplmType_FloatArray, true, 1, ratio};
        const auto runtime = profile::configuration::build(source.value());
        ASSERT_TRUE(runtime.has_value());
        EXPECT_TRUE(runtime.value()->system().volts());
        ASSERT_TRUE(runtime.value()->autopilot().has_value());
        EXPECT_TRUE(runtime.value()->autopilot()->mode().ap());
        ASSERT_TRUE(runtime.value()->system().gear().has_value());
        const auto & bound_gear = runtime.value()->system().gear().value();
        EXPECT_EQ(bound_gear.left_state(), expected);
        EXPECT_EQ(bound_gear.nose_state(), expected);
        EXPECT_EQ(bound_gear.right_state(), expected);
    }
}

TEST(profile_test, boolean_bindings_select_supported_numeric_and_scalar_or_array_accessors)
{
    struct test_case { int mask; bool indexed; const char * accessor; const char * accepted; };
    const test_case cases[] = {
        {xplmType_Int | xplmType_FloatArray, false, "int", "1"},
        {xplmType_Int | xplmType_FloatArray, true, "float array", "1.5"},
        {xplmType_Float | xplmType_IntArray, false, "float", "1.5"},
        {xplmType_Float | xplmType_IntArray, true, "int array", "1"},
        {xplmType_Int | xplmType_IntArray, false, "int", "1"},
        {xplmType_Int | xplmType_IntArray, true, "int array", "1"},
        {xplmType_Float | xplmType_FloatArray, false, "float", "1.5"},
        {xplmType_Float | xplmType_FloatArray, true, "float array", "1.5"},
        {xplmType_IntArray | xplmType_FloatArray, false, "int array", "1"},
        {xplmType_Int, true, "int", "1"},
        {xplmType_Float, true, "float", "1.5"}
    };
    for(const auto & entry : cases) {
        SCOPED_TRACE(std::to_string(entry.mask) + (entry.indexed ? " indexed" : " unindexed"));
        const std::string path = "sim/test/mixed_bool";
        xplm_test::data_refs[path] = {static_cast<XPLMDataTypeID>(entry.mask), true, 1, 1.5};
        auto node = YAML::Load("{key: sim/test/mixed_bool, values: [" + std::string(entry.accepted) + "]}");
        if(entry.indexed) node["index"] = 3;
        const auto key = conf::key::build(node);
        ASSERT_TRUE(key.has_value());
        const auto bound = profile::bool_data_ref::build(key.value());
        ASSERT_TRUE(bound.has_value());
        EXPECT_TRUE(bound.value()->is_set());
        EXPECT_EQ(xplm_test::last_access[path], entry.accessor);
    }
}

TEST(profile_test, float_bindings_ignore_integer_bits_and_use_supported_read_and_write_accessors)
{
    struct test_case { int mask; bool indexed; const char * reader; const char * writer; };
    const test_case cases[] = {
        {xplmType_Int | xplmType_FloatArray, false, "float array", "float array write"},
        {xplmType_Int | xplmType_FloatArray, true, "float array", "float array write"},
        {xplmType_Float | xplmType_IntArray, false, "float", "float write"},
        {xplmType_Float | xplmType_IntArray, true, "float", "float write"},
        {xplmType_Float | xplmType_FloatArray | xplmType_Int, false, "float", "float write"},
        {xplmType_Float | xplmType_FloatArray | xplmType_Int, true, "float array", "float array write"},
        {xplmType_FloatArray, false, "float array", "float array write"},
        {xplmType_Float, true, "float", "float write"}
    };
    for(const auto & entry : cases) {
        SCOPED_TRACE(std::to_string(entry.mask) + (entry.indexed ? " indexed" : " unindexed"));
        const std::string path = "sim/test/mixed_float";
        xplm_test::data_refs[path] = {static_cast<XPLMDataTypeID>(entry.mask), true, 1, 1.5};
        auto node = YAML::Load("{key: sim/test/mixed_float}");
        if(entry.indexed) node["index"] = 3;
        const auto key = conf::key::build(node);
        ASSERT_TRUE(key.has_value());
        const auto bound = profile::float_data_ref::build(key.value());
        ASSERT_TRUE(bound.has_value());
        EXPECT_FLOAT_EQ(bound->get(), 1.5f);
        EXPECT_EQ(xplm_test::last_access[path], entry.reader);
        bound->set(5.75f);
        EXPECT_EQ(xplm_test::last_access[path], entry.writer);
        EXPECT_FLOAT_EQ(bound->get(), 5.75f);
    }
    xplm_test::data_refs["sim/test/integer_only"].type = xplmType_IntArray;
    const auto key = conf::key::build(YAML::Load("sim/test/integer_only"));
    ASSERT_TRUE(key.has_value());
    EXPECT_FALSE(profile::float_data_ref::build(key.value()).has_value());
}

TEST(profile_test, float_values_match_within_tolerance)
{
    const std::string path = "sim/test/float_values";
    const auto key = conf::key::build(YAML::Load("{key: sim/test/float_values, values: [0.3, 1000]}"));
    ASSERT_TRUE(key.has_value());
    const struct { double value; bool set; } cases[] = {
        {0.3, true}, {0.30004, true}, {0.3002, false}, {1000.05, true}, {1000.5, false}, {0.0, false}
    };
    for(const auto & entry : cases) {
        SCOPED_TRACE(entry.value);
        xplm_test::data_refs[path] = {xplmType_Float, true, 1, entry.value};
        const auto bound = profile::bool_data_ref::build(key.value());
        ASSERT_TRUE(bound.has_value());
        EXPECT_EQ(bound.value()->is_set(), entry.set);
    }
}

TEST(profile_test, warns_when_a_dial_dataref_is_read_only)
{
    const auto key = conf::key::build(YAML::Load("sim/test/readonly_dial"));
    ASSERT_TRUE(key.has_value());
    xplm_test::data_refs["sim/test/readonly_dial"] = {xplmType_Float, true, 1, 0, true};
    xplm_test::log.clear();
    ASSERT_TRUE(profile::float_data_ref::build(key.value()).has_value());
    EXPECT_EQ(xplm_test::log.find("read-only"), std::string::npos);

    xplm_test::data_refs["sim/test/readonly_dial"].writable = false;
    xplm_test::log.clear();
    EXPECT_TRUE(profile::float_data_ref::build(key.value()).has_value());
    EXPECT_NE(xplm_test::log.find("Dial DataRef 'sim/test/readonly_dial' is read-only"), std::string::npos);
}

static size_t count_occurrences(const std::string & text, const std::string & needle)
{
    size_t count = 0;
    for(auto at = text.find(needle); at != std::string::npos; at = text.find(needle, at + 1)) ++count;
    return count;
}

TEST(profile_test, repeated_indexed_read_failures_are_logged_once_until_a_read_succeeds)
{
    const auto key = conf::key::build(YAML::Load("{key: sim/test/failing_array, index: 2}"));
    ASSERT_TRUE(key.has_value());
    xplm_test::data_refs["sim/test/failing_array"] = {xplmType_FloatArray, true, 0, 1.0};
    const auto bound = profile::float_data_ref::build(key.value());
    ASSERT_TRUE(bound.has_value());

    xplm_test::log.clear();
    for(int n = 0; n < 5; ++n) bound->get();
    EXPECT_EQ(count_occurrences(xplm_test::log, "Failed to read indexed float DataRef"), 1u);

    // A successful read re-arms the message for the next failure.
    bound->data_ref()->array_read_count = 1;
    bound->get();
    bound->data_ref()->array_read_count = 0;
    for(int n = 0; n < 3; ++n) bound->get();
    EXPECT_EQ(count_occurrences(xplm_test::log, "Failed to read indexed float DataRef"), 2u);
}

} // namespace profile_test
