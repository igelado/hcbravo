// SPDX-License-Identifier: LGPL-2.1-only
//
// Tests for runtime profiles built from parsed configurations.

#include <gtest/gtest.h>

#define HCBRAVO_PROFILE_TESTS
#include <conf.h>
#include <profile.h>

#include <filesystem>
#include <fstream>

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
