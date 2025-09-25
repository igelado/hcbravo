#include <gtest/gtest.h>

#define HCBRAVO_PROFILE_TESTS
#include <profile.h>


TEST(profile_test, bool_data_ref) {
    auto node = YAML::Load(R"(
tag:
  - key: 'sim/test/bool'
    )");
    auto data_ref = value_data_ref(node["tag"]);
    ASSERT_FALSE(data_ref.data().empty());

    ASSERT_EQ(data_ref.data().front()->data_ref()->name, "sim/test/bool");
}

TEST(profile_test, bool_data_ref_unset) {
    auto node = YAML::Load(R"(
tag:
  - key: 'sim/test/bool'
    )");
    auto data_ref = value_data_ref(node["tag"]);
    ASSERT_FALSE(data_ref.data().empty());
    ASSERT_FALSE(data_ref.is_set());
}


TEST(profile_test, bool_data_ref_set) {
    auto node = YAML::Load(R"(
tag:
  - key: 'sim/test/bool'
    )");
    auto data_ref = value_data_ref(node["tag"]);
    ASSERT_FALSE(data_ref.data().empty());
    data_ref.data().front()->data_ref()->value = 1;
    ASSERT_TRUE(data_ref.is_set());
}



TEST(profile_test, invalid_data_ref) {
    auto node = YAML::Load(R"(
tag:
  - no_key: 'something/else'
    )");
    auto data_ref = value_data_ref(node["tag"]);
    ASSERT_TRUE(data_ref.data().empty());
}

TEST(profile_test, multiple_bool_data_ref) {
    auto node = YAML::Load(R"(
tag:
  - key: 'sim/test/first'
  - key: 'sim/test/second'
    )");
    auto data_ref = value_data_ref(node["tag"]);
    ASSERT_EQ(data_ref.data().size(), 2);
    
    ASSERT_EQ(data_ref.data()[0]->data_ref()->name, "sim/test/first");
    ASSERT_EQ(data_ref.data()[1]->data_ref()->name, "sim/test/second");
}

TEST(profile_test, int_data_ref) {
    auto node = YAML::Load(R"(
tag:
  - key: 'sim/test/unsigned'
    type: int
    values:
      - 1
      - 2
      - 5
    )");
    auto data_ref = value_data_ref(node["tag"]);
    ASSERT_FALSE(data_ref.data().empty());
    ASSERT_EQ(data_ref.data().front()->data_ref()->name, "sim/test/unsigned");
}

TEST(profile_test, int_data_ref_set) {
    auto node = YAML::Load(R"(
tag:
  - key: 'sim/test/unsigned'
    type: int
    values:
      - 1
      - 2
      - 5
    )");
    auto data_ref = value_data_ref(node["tag"]);
    ASSERT_FALSE(data_ref.data().empty());

    data_ref.data().front()->data_ref()->value = 1;
    ASSERT_TRUE(data_ref.is_set());

    data_ref.data().front()->data_ref()->value = 2;
    ASSERT_TRUE(data_ref.is_set());

    data_ref.data().front()->data_ref()->value = 5;
    ASSERT_TRUE(data_ref.is_set());
}

TEST(profile_test, int_data_ref_unset) {
    auto node = YAML::Load(R"(
tag:
  - key: 'sim/test/unsigned'
    type: int
    values:
      - 1
      - 2
      - 5
    )");
    auto data_ref = value_data_ref(node["tag"]);
    ASSERT_FALSE(data_ref.data().empty());
    ASSERT_FALSE(data_ref.is_set());

    data_ref.data().front()->data_ref()->value = 4;
    ASSERT_FALSE(data_ref.is_set());
}

TEST(profile_test, autopilot) {
    auto node = YAML::Load(R"(
autopilot:
 hdg:
  - key: 'sim/cockpit2/autopilot/heading_mode'
    type: int
    values: 
      - 15
      - 13
      - 2
 nav:
  - key: 'sim/cockpit2/autopilot/nav_status'
  - key: 'sim/cockpit2/autopilot/gpss_status'
 apr:
  - key: 'sim/cockpit2/autopilot/approach_status'
 rev:
  - key: 'sim/cockpit2/autopilot/backcourse_status'
 alt:
  - key: 'sim/cockpit2/autopilot/altitude_hold_status'
 vs:
  - key: 'sim/cockpit2/autopilot/vvi_status'
 ias:
  - key: 'sim/cockpit2/autopilot/speed_status'
 ap:
  - key: 'sim/cockpit2/autopilot/servos_on'
    )");

    auto data_ref = autopilot_data_ref(node["autopilot"]);
    ASSERT_FALSE(data_ref.hdg());
    ASSERT_FALSE(data_ref.nav());
    ASSERT_FALSE(data_ref.apr());
    ASSERT_FALSE(data_ref.rev());
    ASSERT_FALSE(data_ref.alt());
    ASSERT_FALSE(data_ref.vs());
    ASSERT_FALSE(data_ref.ias());
    ASSERT_FALSE(data_ref.ap());

    data_ref.hdg_data_ref().data().front()->data_ref()->value = 13;
    ASSERT_TRUE(data_ref.hdg());

    data_ref.nav_data_ref().data()[0]->data_ref()->value = 1;
    ASSERT_TRUE(data_ref.nav());
    data_ref.nav_data_ref().data()[0]->data_ref()->value = 0;
    data_ref.nav_data_ref().data()[0]->data_ref()->value = 1;
    ASSERT_TRUE(data_ref.nav());

    data_ref.apr_data_ref().data().front()->data_ref()->value = 1;
    ASSERT_TRUE(data_ref.apr());

    data_ref.rev_data_ref().data().front()->data_ref()->value = 1;
    ASSERT_TRUE(data_ref.rev());

    data_ref.alt_data_ref().data().front()->data_ref()->value = 1;
    ASSERT_TRUE(data_ref.alt());

    data_ref.vs_data_ref().data().front()->data_ref()->value = 1;
    ASSERT_TRUE(data_ref.vs());

    data_ref.ias_data_ref().data().front()->data_ref()->value = 1;
    ASSERT_TRUE(data_ref.ias());

    data_ref.ap_data_ref().data().front()->data_ref()->value = 1;
    ASSERT_TRUE(data_ref.ap());
}

TEST(profile_test, system) {
    auto node = YAML::Load(R"(
system:
 volts:
  - key: 'sim/cockpit2/electrical/bus_volts'
 gear:
  - key: 'sim/flightmodel2/gear/deploy_ratio'

    )");

    auto data_ref = system_data_ref(node["system"]);
    ASSERT_FALSE(data_ref.volts());
    ASSERT_FALSE(data_ref.gear());

    data_ref.volts_data_ref().data().front()->data_ref()->value = 1;
    ASSERT_TRUE(data_ref.volts());

    data_ref.gear_data_ref().data().front()->data_ref()->value = 1;
    ASSERT_TRUE(data_ref.gear());
}