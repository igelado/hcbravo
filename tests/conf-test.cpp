// SPDX-License-Identifier: LGPL-2.1-only
//
// Tests for YAML-only configuration parsing.

#include <gtest/gtest.h>

#include <conf.h>

#include <filesystem>
#include <fstream>

static
std::expected<conf::configuration, int>
read_configuration(const std::string & contents)
{
    const auto path = std::filesystem::temp_directory_path() / "hcbravo-conf-test.yaml";
    {
        std::ofstream file(path);
        if(!file) return std::unexpected(0);
        file << contents;
    }

    auto configuration = conf::configuration::read(path.string());
    std::filesystem::remove(path);
    return configuration;
}

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
values:
  - 1
  - 14
)");

    const auto data_ref = conf::key::build(node);

    ASSERT_TRUE(data_ref.has_value());
    EXPECT_EQ(data_ref->path(), "sim/test/mapped");
    EXPECT_TRUE(data_ref->inverted());
    ASSERT_TRUE(data_ref->index().has_value());
    EXPECT_EQ(data_ref->index().value(), 2);
    EXPECT_EQ(data_ref->values(), (std::vector<std::string>{"1", "14"}));
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

TEST(conf_test, mapped_data_ref_rejects_obsolete_type_property)
{
    const auto node = YAML::Load(R"(
key: sim/test/mapped
type: int
)");

    EXPECT_FALSE(conf::key::build(node).has_value());
}

TEST(conf_test, configuration_maps_reject_unknown_keys)
{
    EXPECT_FALSE(conf::airspeed::build(YAML::Load(R"(
is_mach: sim/test/is_mach
value: sim/test/value
units: knots
)")).has_value());

    EXPECT_FALSE(conf::autopilot_dial::build(YAML::Load(R"(
crs: sim/test/course
headingg: sim/test/heading
)")).has_value());

    EXPECT_FALSE(conf::autopilot_mode::build(YAML::Load(R"(
ap: sim/test/ap
heading: sim/test/heading
)")).has_value());

    EXPECT_FALSE(conf::autopilot::build(YAML::Load(R"(
modes:
  ap: sim/test/ap
mode: sim/test/legacy
)")).has_value());

    EXPECT_FALSE(conf::gear_signals::build(YAML::Load(R"(
safe: sim/test/safe
unsafe: sim/test/unsafe
up: sim/test/up
)")).has_value());

    EXPECT_FALSE(conf::gear::build(YAML::Load(R"(
left:
  safe: sim/test/left-safe
  unsafe: sim/test/left-unsafe
nose:
  safe: sim/test/nose-safe
  unsafe: sim/test/nose-unsafe
right:
  safe: sim/test/right-safe
  unsafe: sim/test/right-unsafe
center: sim/test/center
)")).has_value());

    EXPECT_FALSE(conf::system::build(YAML::Load(R"(
volts: sim/test/volts
voltage: sim/test/voltage
)")).has_value());

    EXPECT_FALSE(conf::annunciator::build(YAML::Load(R"(
master_warn: sim/test/master-warn
master_warning: sim/test/master-warning
)")).has_value());

    EXPECT_FALSE(read_configuration(R"(
name: Valid
aircrafts: [Valid]
models: [VALID]
system:
  volts: sim/test/volts
systems:
  volts: sim/test/other-volts
)").has_value());
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

TEST(conf_test, autopilot_dials_reject_ambiguous_aliases)
{
    const auto course_aliases = YAML::Load(R"(
crs: sim/test/course-short
course: sim/test/course-long
)");
    const auto heading_aliases = YAML::Load(R"(
hdg: sim/test/heading-short
heading: sim/test/heading-long
)");

    EXPECT_FALSE(conf::autopilot_dial::build(course_aliases).has_value());
    EXPECT_FALSE(conf::autopilot_dial::build(heading_aliases).has_value());
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

TEST(conf_test, data_ref_values_preserve_float_literals)
{
    const auto node = YAML::Load(R"(
key: sim/test/float
values:
  - 1.25
  - 2.5
)");

    const auto data_ref = conf::key::build(node);

    ASSERT_TRUE(data_ref.has_value());
    EXPECT_EQ(data_ref->values(), (std::vector<std::string>{"1.25", "2.5"}));
}

TEST(conf_test, data_ref_values_must_be_a_non_empty_scalar_sequence)
{
    const auto scalar_values = YAML::Load(R"(
key: sim/test/scalar-values
values: 1
)");
    const auto empty_values = YAML::Load(R"(
key: sim/test/empty-values
values: []
)");
    const auto non_scalar_values = YAML::Load(R"(
key: sim/test/non-scalar-values
values:
  - nested: value
)");

    EXPECT_FALSE(conf::key::build(scalar_values).has_value());
    EXPECT_FALSE(conf::key::build(empty_values).has_value());
    EXPECT_FALSE(conf::key::build(non_scalar_values).has_value());
}

TEST(conf_test, data_ref_value_rejects_empty_sequence)
{
    const auto node = YAML::Load("[]");

    EXPECT_FALSE(conf::value::build(node).has_value());
}

TEST(conf_test, required_system_volts_rejects_empty_sequence)
{
    const auto node = YAML::Load(R"(
volts: []
)");

    EXPECT_FALSE(conf::system::build(node).has_value());
}

TEST(conf_test, gear_uses_safe_and_unsafe_signals_for_each_wheel)
{
    const auto node = YAML::Load(R"(
left:
  safe: sim/test/left-safe
  unsafe: sim/test/left-unsafe
nose:
  safe: sim/test/nose-safe
  unsafe: sim/test/nose-unsafe
right:
  safe: sim/test/right-safe
  unsafe: sim/test/right-unsafe
)");

    const auto gear = conf::gear::build(node);

    ASSERT_TRUE(gear.has_value());
    EXPECT_EQ(gear->left().safe().keys().front().path(), "sim/test/left-safe");
    EXPECT_EQ(gear->left().unsafe().keys().front().path(), "sim/test/left-unsafe");
    EXPECT_EQ(gear->nose().safe().keys().front().path(), "sim/test/nose-safe");
    EXPECT_EQ(gear->nose().unsafe().keys().front().path(), "sim/test/nose-unsafe");
    EXPECT_EQ(gear->right().safe().keys().front().path(), "sim/test/right-safe");
    EXPECT_EQ(gear->right().unsafe().keys().front().path(), "sim/test/right-unsafe");
}

TEST(conf_test, bundled_configurations_parse)
{
    const auto conf_dir = std::filesystem::path(HCBRAVO_SOURCE_DIR) / "conf";
    size_t configuration_count = 0;

    for(const auto & entry : std::filesystem::directory_iterator(conf_dir)) {
        if(entry.path().extension() != ".yaml") continue;

        const auto configuration = conf::configuration::read(entry.path().string());
        ASSERT_TRUE(configuration.has_value()) << entry.path();
        ++configuration_count;
    }

    EXPECT_GT(configuration_count, 0);
}

TEST(conf_test, configuration_requires_valid_top_level_identity)
{
    const auto valid = read_configuration(R"(
name: Valid
aircrafts: [Valid Aircraft]
models: [VALD]
system:
  volts: sim/test/volts
)");
    const auto non_map_root = read_configuration("- invalid");
    const auto missing_name = read_configuration(R"(
aircrafts: [Valid Aircraft]
models: [VALD]
system:
  volts: sim/test/volts
)");
    const auto empty_name = read_configuration(R"(
name: ''
aircrafts: [Valid Aircraft]
models: [VALD]
system:
  volts: sim/test/volts
)");

    ASSERT_TRUE(valid.has_value());
    EXPECT_EQ(valid->name(), "Valid");
    EXPECT_FALSE(non_map_root.has_value());
    EXPECT_FALSE(missing_name.has_value());
    EXPECT_FALSE(empty_name.has_value());
}

TEST(conf_test, configuration_requires_non_empty_scalar_aircrafts_and_models)
{
    const auto missing_aircrafts = read_configuration(R"(
name: Invalid
models: [VALD]
system:
  volts: sim/test/volts
)");
    const auto invalid_aircraft = read_configuration(R"(
name: Invalid
aircrafts: [""]
models: [VALD]
system:
  volts: sim/test/volts
)");
    const auto empty_models = read_configuration(R"(
name: Invalid
aircrafts: [Valid Aircraft]
models: []
system:
  volts: sim/test/volts
)");
    const auto invalid_model = read_configuration(R"(
name: Invalid
aircrafts: [Valid Aircraft]
models:
  - name: VALD
system:
  volts: sim/test/volts
)");

    EXPECT_FALSE(missing_aircrafts.has_value());
    EXPECT_FALSE(invalid_aircraft.has_value());
    EXPECT_FALSE(empty_models.has_value());
    EXPECT_FALSE(invalid_model.has_value());
}
