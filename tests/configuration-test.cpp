// SPDX-License-Identifier: LGPL-2.1-only
#include "conf.h"
#include "configuration-files.h"

#include <gtest/gtest.h>
#include <yaml-cpp/yaml.h>
#include <filesystem>
#include <fstream>

namespace configuration_test {

class ConfigurationTest : public ::testing::TestWithParam<configuration_files::configuration_case> {};

TEST_P(ConfigurationTest, Valid) {
    const auto result = conf::validate_file(GetParam().path);
    ASSERT_TRUE(result.has_value()) << GetParam().path << ": " << result.error();
}

INSTANTIATE_TEST_SUITE_P(Bundled, ConfigurationTest,
    ::testing::ValuesIn(configuration_files::configuration_files),
    [](const ::testing::TestParamInfo<configuration_files::configuration_case> & info) { return info.param.name; });

TEST(ValidationTest, MissingFile) {
    const auto result = conf::validate_file("hcbravo-nonexistent-profile.yaml");
    ASSERT_FALSE(result.has_value());
    EXPECT_FALSE(result.error().empty());
}

static std::expected<void, std::string> validate_contents(const std::string & contents) {
    const auto path = std::filesystem::temp_directory_path() / "hcbravo-validation-test.yaml";
    { std::ofstream file(path); file << contents; }
    const auto result = conf::validate_file(path.string());
    std::filesystem::remove(path);
    return result;
}

TEST(ValidationTest, RejectsInvalidProfiles) {
    for(const auto invalid : {
        "name: ''", "aircrafts: []", "models: invalid", "system: []",
        "system: {volts: []}", "system: {volts: {key: ''}}",
        "system: {volts: {key: sim/test, index: -1}}",
        "system: {volts: {key: sim/test, type: int}}",
        "system: {volts: {key: sim/test, invert: invalid}}",
        "system: {volts: {key: sim/test, values: []}}",
        "system: {volts: sim/test, gear: {left: {safe: sim/safe}}}",
        "autopilot: {modes: {}}", "autopilot: {modes: {ap: []}}",
        "autopilot: {modes: {ap: sim/ap}, dials: {ias: {value: sim/value}}}",
        "annunciator: {master_warn: []}"
    }) {
        SCOPED_TRACE(invalid);
        auto node = YAML::Load("name: Test\naircrafts: [Test]\nmodels: [TEST]\nsystem: {volts: sim/volts}");
        for(const auto & entry : YAML::Load(invalid)) node[entry.first.as<std::string>()] = entry.second;
        const auto result = validate_contents(YAML::Dump(node));
        ASSERT_FALSE(result.has_value());
        EXPECT_FALSE(result.error().empty());
    }
}

TEST(ValidationTest, AcceptsMinimalProfile) {
    const auto result = validate_contents(
        "name: Test\naircrafts: [Test]\nmodels: [TEST]\nsystem: {volts: sim/volts}");
    ASSERT_TRUE(result.has_value()) << result.error();
}

TEST(ValidationTest, RejectsMultipleDialDataRefs) {
    const std::string prefix = "name: Test\naircrafts: [Test]\nmodels: [TEST]\nsystem: {volts: sim/volts}\n"
        "autopilot:\n  modes: {ap: sim/ap}\n  dials:\n    ";
    for(const auto label : {"crs", "course", "hdg", "heading", "vs", "alt"}) {
        SCOPED_TRACE(label);
        EXPECT_FALSE(validate_contents(prefix + label + ": [sim/a, sim/b]").has_value());
        EXPECT_TRUE(validate_contents(prefix + label + ": [sim/a]").has_value());
    }
    EXPECT_FALSE(validate_contents(prefix + "ias: {is_mach: sim/mach, value: [sim/a, sim/b]}").has_value());
    EXPECT_TRUE(validate_contents(prefix + "ias: {is_mach: sim/mach, value: [sim/a]}").has_value());
}

TEST(ValidationTest, RejectsMalformedYaml) {
    EXPECT_FALSE(validate_contents("name: [unterminated").has_value());
}

} // namespace configuration_test
