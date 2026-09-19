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
