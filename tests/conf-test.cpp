// SPDX-License-Identifier: LGPL-2.1-only
//
// Tests for YAML-only configuration parsing.

#include <gtest/gtest.h>

#include <conf.h>

TEST(conf_test, scalar_data_ref_is_valid)
{
    const auto node = YAML::Load("'sim/test/scalar'");

    const auto data_ref = conf::key::build(node);

    ASSERT_TRUE(data_ref.has_value());
    EXPECT_EQ(data_ref->path(), "sim/test/scalar");
    EXPECT_FALSE(data_ref->inverted());
    EXPECT_FALSE(data_ref->index().has_value());
}

TEST(conf_test, mapped_data_ref_is_valid)
{
    const auto node = YAML::Load(R"(
key: sim/test/mapped
invert: true
index: 2
)");

    const auto data_ref = conf::key::build(node);

    ASSERT_TRUE(data_ref.has_value());
    EXPECT_EQ(data_ref->path(), "sim/test/mapped");
    EXPECT_TRUE(data_ref->inverted());
    ASSERT_TRUE(data_ref->index().has_value());
    EXPECT_EQ(data_ref->index().value(), 2);
}

TEST(conf_test, sequence_parses_each_data_ref)
{
    const auto node = YAML::Load(R"(
- sim/test/first
- key: sim/test/second
  invert: true
)");

    const auto value = conf::value::build(node);

    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(value->keys().size(), 2);
}

TEST(conf_test, mapped_data_ref_requires_scalar_key)
{
    const auto node = YAML::Load(R"(
invert: true
)");

    const auto data_ref = conf::key::build(node);

    EXPECT_FALSE(data_ref.has_value());
}

TEST(conf_test, negative_index_is_invalid)
{
    const auto node = YAML::Load(R"(
key: sim/test/indexed
index: -1
)");

    const auto data_ref = conf::key::build(node);

    EXPECT_FALSE(data_ref.has_value());
}

TEST(conf_test, autopilot_uses_modes_key)
{
    const auto node = YAML::Load(R"(
modes:
  ap: sim/test/autopilot
)");

    const auto autopilot = conf::autopilot::build(node);

    EXPECT_TRUE(autopilot.has_value());
}

TEST(conf_test, autopilot_rejects_legacy_mode_key)
{
    const auto node = YAML::Load(R"(
mode:
  ap: sim/test/autopilot
)");

    const auto autopilot = conf::autopilot::build(node);

    EXPECT_FALSE(autopilot.has_value());
}

TEST(conf_test, autopilot_dials_accept_long_aliases)
{
    const auto node = YAML::Load(R"(
course: sim/test/course
heading: sim/test/heading
)");

    const auto dials = conf::autopilot_dial::build(node);

    ASSERT_TRUE(dials.has_value());
    EXPECT_TRUE(dials->course().has_value());
    EXPECT_TRUE(dials->heading().has_value());
}

TEST(conf_test, autopilot_dials_reject_invalid_primary_alias)
{
    const auto node = YAML::Load(R"(
crs:
  not_a_key: sim/test/invalid
course: sim/test/course
)");

    const auto dials = conf::autopilot_dial::build(node);

    EXPECT_FALSE(dials.has_value());
}

TEST(conf_test, airspeed_exposes_its_data_ref_descriptors)
{
    const auto node = YAML::Load(R"(
is_mach: sim/test/is_mach
value: sim/test/airspeed
)");

    const auto airspeed = conf::airspeed::build(node);

    ASSERT_TRUE(airspeed.has_value());
    EXPECT_EQ(airspeed->is_mach().path(), "sim/test/is_mach");
    ASSERT_EQ(airspeed->value().keys().size(), 1);
    EXPECT_EQ(airspeed->value().keys().front().path(), "sim/test/airspeed");
}
