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
