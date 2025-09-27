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

    auto data_ref_opt = autopilot_mode_data_ref::build(node["autopilot"]);
    ASSERT_TRUE(data_ref_opt.has_value());

    auto data_ref = std::move(data_ref_opt.value());
    ASSERT_TRUE(data_ref.hdg().has_value());
    ASSERT_FALSE(data_ref.hdg().value());

    ASSERT_TRUE(data_ref.nav().has_value());
    ASSERT_FALSE(data_ref.nav().value());

    ASSERT_TRUE(data_ref.apr().has_value());
    ASSERT_FALSE(data_ref.apr().value());

    ASSERT_TRUE(data_ref.rev().has_value());
    ASSERT_FALSE(data_ref.rev().value());

    ASSERT_TRUE(data_ref.alt().has_value());
    ASSERT_FALSE(data_ref.alt().value());

    ASSERT_TRUE(data_ref.vs().has_value());
    ASSERT_FALSE(data_ref.vs().value());

    ASSERT_TRUE(data_ref.ias().has_value());
    ASSERT_FALSE(data_ref.ias().value());

    ASSERT_FALSE(data_ref.ap());

    ASSERT_TRUE(data_ref.hdg_data_ref().has_value());
    data_ref.hdg_data_ref().value().data().front()->data_ref()->value = 13;
    ASSERT_TRUE(data_ref.hdg().value());

    ASSERT_TRUE(data_ref.nav_data_ref().has_value());
    data_ref.nav_data_ref().value().data()[0]->data_ref()->value = 1;
    ASSERT_TRUE(data_ref.nav().value());
    data_ref.nav_data_ref().value().data()[0]->data_ref()->value = 0;
    data_ref.nav_data_ref().value().data()[0]->data_ref()->value = 1;
    ASSERT_TRUE(data_ref.nav().value());

    ASSERT_TRUE(data_ref.apr_data_ref().has_value());
    data_ref.apr_data_ref().value().data().front()->data_ref()->value = 1;
    ASSERT_TRUE(data_ref.apr().value());

    ASSERT_TRUE(data_ref.rev_data_ref().has_value());
    data_ref.rev_data_ref().value().data().front()->data_ref()->value = 1;
    ASSERT_TRUE(data_ref.rev().value());

    ASSERT_TRUE(data_ref.alt_data_ref().has_value());
    data_ref.alt_data_ref().value().data().front()->data_ref()->value = 1;
    ASSERT_TRUE(data_ref.alt().value());

    ASSERT_TRUE(data_ref.vs_data_ref().has_value());
    data_ref.vs_data_ref().value().data().front()->data_ref()->value = 1;
    ASSERT_TRUE(data_ref.vs().value());

    ASSERT_TRUE(data_ref.ias_data_ref().has_value());
    data_ref.ias_data_ref().value().data().front()->data_ref()->value = 1;
    ASSERT_TRUE(data_ref.ias().value());

    data_ref.ap_data_ref().data().front()->data_ref()->value = 1;
    ASSERT_TRUE(data_ref.ap());
}

TEST(profile_test, autopilot_minimal) {
    auto node = YAML::Load(R"(
autopilot:
 ap:
  - key: 'sim/cockpit2/autopilot/servos_on'
    )");
    auto data_ref_opt = autopilot_mode_data_ref::build(node["autopilot"]);
    ASSERT_TRUE(data_ref_opt.has_value());

    auto data_ref = std::move(data_ref_opt.value());
    ASSERT_FALSE(data_ref.hdg().has_value());
    ASSERT_FALSE(data_ref.nav().has_value());
    ASSERT_FALSE(data_ref.apr().has_value());
    ASSERT_FALSE(data_ref.rev().has_value());
    ASSERT_FALSE(data_ref.alt().has_value());
    ASSERT_FALSE(data_ref.vs().has_value());
    ASSERT_FALSE(data_ref.ias().has_value());
    ASSERT_FALSE(data_ref.ap());

    data_ref.ap_data_ref().data().front()->data_ref()->value = 1;
    ASSERT_TRUE(data_ref.ap());
}

TEST(profile_test, autopilot_kap140) {
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
 alt:
  - key: 'sim/cockpit2/autopilot/altitude_hold_status'
 vs:
  - key: 'sim/cockpit2/autopilot/vvi_status'
 ap:
  - key: 'sim/cockpit2/autopilot/servos_on'
    )");

    auto data_ref_opt = autopilot_mode_data_ref::build(node["autopilot"]);
    ASSERT_TRUE(data_ref_opt.has_value());

    auto data_ref = std::move(data_ref_opt.value());
 
    ASSERT_TRUE(data_ref.hdg().has_value());
    ASSERT_FALSE(data_ref.hdg().value());

    ASSERT_TRUE(data_ref.nav().has_value());
    ASSERT_FALSE(data_ref.nav().value());

    ASSERT_TRUE(data_ref.apr().has_value());
    ASSERT_FALSE(data_ref.apr().value());

    ASSERT_TRUE(data_ref.alt().has_value());
    ASSERT_FALSE(data_ref.alt().value());

    ASSERT_TRUE(data_ref.vs().has_value());
    ASSERT_FALSE(data_ref.vs().value());

    ASSERT_FALSE(data_ref.rev().has_value());
    ASSERT_FALSE(data_ref.ias().has_value());

    ASSERT_FALSE(data_ref.ap());

    ASSERT_TRUE(data_ref.hdg_data_ref().has_value());
    data_ref.hdg_data_ref().value().data().front()->data_ref()->value = 13;
    ASSERT_TRUE(data_ref.hdg().value());

    ASSERT_TRUE(data_ref.nav_data_ref().has_value());
    data_ref.nav_data_ref().value().data()[0]->data_ref()->value = 1;
    ASSERT_TRUE(data_ref.nav().value());
    data_ref.nav_data_ref().value().data()[0]->data_ref()->value = 0;
    data_ref.nav_data_ref().value().data()[0]->data_ref()->value = 1;
    ASSERT_TRUE(data_ref.nav().value());

    ASSERT_TRUE(data_ref.apr_data_ref().has_value());
    data_ref.apr_data_ref().value().data().front()->data_ref()->value = 1;
    ASSERT_TRUE(data_ref.apr().value());

    ASSERT_TRUE(data_ref.alt_data_ref().has_value());
    data_ref.alt_data_ref().value().data().front()->data_ref()->value = 1;
    ASSERT_TRUE(data_ref.alt().value());

    ASSERT_TRUE(data_ref.vs_data_ref().has_value());
    data_ref.vs_data_ref().value().data().front()->data_ref()->value = 1;
    ASSERT_TRUE(data_ref.vs().value());

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

    auto data_ref_opt = system_data_ref::build(node["system"]);
    ASSERT_TRUE(data_ref_opt.has_value());

    auto data_ref = std::move(data_ref_opt.value());
    ASSERT_FALSE(data_ref.volts());
    ASSERT_TRUE(data_ref.gear().has_value());
    ASSERT_FALSE(data_ref.gear().value());

    data_ref.volts_data_ref().data().front()->data_ref()->value = 1;
    ASSERT_TRUE(data_ref.volts());

    data_ref.gear_data_ref().value().data().front()->data_ref()->value = 1;
    ASSERT_TRUE(data_ref.gear());
}

TEST(profile_test, system_no_gear) {
    auto node = YAML::Load(R"(
system:
 volts:
  - key: 'sim/cockpit2/electrical/bus_volts'
    )");

    auto data_ref_opt = system_data_ref::build(node["system"]);
    ASSERT_TRUE(data_ref_opt.has_value());

    auto data_ref = std::move(data_ref_opt.value());
    ASSERT_FALSE(data_ref.volts());
    ASSERT_FALSE(data_ref.gear().has_value());

    data_ref.volts_data_ref().data().front()->data_ref()->value = 1;
    ASSERT_TRUE(data_ref.volts());
}

TEST(profile_test, annunciator) {
  auto node = YAML::Load(R"(
annunciator:
 master_warn:
  - key: 'sim/cockpit2/annunciators/master_warning'
 eng_fire:
  - key: 'sim/cockpit2/annunciators/engine_fires'
 oil_low:
  - key: 'sim/cockpit2/annunciators/oil_pressure_low'
 fuel_low:
  - key: 'sim/cockpit2/annunciators/fuel_pressure_low'
 anti_ice:
  - key: 'sim/cockpit2/annunciators/pitot_heat'
 starter:
  - key: 'sim/cockpit2/engine/actuators/starter_hit'
 apu: 
  - key: 'sim/cockpit2/electrical/APU_running'
 master_caution:
  - key: 'sim/cockpit2/annunciators/master_caution'
 vacuum_low:
  - key: 'sim/cockpit2/annunciators/low_vacuum'
 hydro_low:
  - key: 'sim/cockpit2/annunciators/hydraulic_pressure'
 aux_fuel:
  - key: 'sim/cockpit2/fuel/transfer_pump_left'
  - key: 'sim/cockpit2/fuel/transfer_pump_right'
 parking_brake:
   - key: 'sim/cockpit2/controls/parking_brake_ratio'
 volt_low:
  - key: 'sim/cockpit2/annunciators/low_voltage'
 door_open:
  - key: 'sim/flightmodel2/misc/canopy_open_ratio'
  - key: 'sim/flightmodel2/misc/door_open_ratio'
  - key: 'sim/cockpit2/annunciators/cabin_door_open'

    )");


    auto data_ref_opt = annunciator_data_ref::build(node["annunciator"]);
    ASSERT_TRUE(data_ref_opt.has_value());
    auto data_ref = std::move(data_ref_opt.value());

    ASSERT_TRUE(data_ref.master_warn().has_value());
    ASSERT_FALSE(data_ref.master_warn().value());
    data_ref.master_warn_data_ref().value().data().front()->data_ref()->value = 1;
    ASSERT_TRUE(data_ref.master_warn().value());

    ASSERT_TRUE(data_ref.eng_fire().has_value());
    ASSERT_FALSE(data_ref.eng_fire().value());
    data_ref.eng_fire_data_ref().value().data().front()->data_ref()->value = 1;
    ASSERT_TRUE(data_ref.eng_fire().value());

    ASSERT_TRUE(data_ref.oil_low().has_value());
    ASSERT_FALSE(data_ref.oil_low().value());
    data_ref.oil_low_data_ref().value().data().front()->data_ref()->value = 1;
    ASSERT_TRUE(data_ref.oil_low().value());

    ASSERT_TRUE(data_ref.fuel_low().has_value());
    ASSERT_FALSE(data_ref.fuel_low().value());
    data_ref.fuel_low_data_ref().value().data().front()->data_ref()->value = 1;
    ASSERT_TRUE(data_ref.fuel_low().value());

    ASSERT_TRUE(data_ref.anti_ice().has_value());
    ASSERT_FALSE(data_ref.anti_ice().value());
    data_ref.anti_ice_data_ref().value().data().front()->data_ref()->value = 1;
    ASSERT_TRUE(data_ref.anti_ice().value());

    ASSERT_TRUE(data_ref.starter().has_value());
    ASSERT_FALSE(data_ref.starter().value());
    data_ref.starter_data_ref().value().data().front()->data_ref()->value = 1;
    ASSERT_TRUE(data_ref.starter().value());

    ASSERT_TRUE(data_ref.apu().has_value());
    ASSERT_FALSE(data_ref.apu().value());
    data_ref.apu_data_ref().value().data().front()->data_ref()->value = 1;
    ASSERT_TRUE(data_ref.apu().value());

    ASSERT_TRUE(data_ref.master_caution().has_value());
    ASSERT_FALSE(data_ref.master_caution().value());
    data_ref.master_caution_data_ref().value().data().front()->data_ref()->value = 1;
    ASSERT_TRUE(data_ref.master_caution().value());

    ASSERT_TRUE(data_ref.vacuum_low().has_value());
    ASSERT_FALSE(data_ref.vacuum_low().value());
    data_ref.vacuum_low_data_ref().value().data().front()->data_ref()->value = 1;
    ASSERT_TRUE(data_ref.vacuum_low().value());

    ASSERT_TRUE(data_ref.hydro_low().has_value());
    ASSERT_FALSE(data_ref.hydro_low().value());
    data_ref.hydro_low_data_ref().value().data().front()->data_ref()->value = 1;
    ASSERT_TRUE(data_ref.hydro_low().value());

    ASSERT_TRUE(data_ref.parking_brake().has_value());
    ASSERT_FALSE(data_ref.parking_brake().value());
    data_ref.parking_brake_data_ref().value().data().front()->data_ref()->value = 1;
    ASSERT_TRUE(data_ref.parking_brake().value());

    ASSERT_TRUE(data_ref.volt_low().has_value());
    ASSERT_FALSE(data_ref.volt_low().value());
    data_ref.volt_low_data_ref().value().data().front()->data_ref()->value = 1;
    ASSERT_TRUE(data_ref.volt_low().value());

    ASSERT_TRUE(data_ref.aux_fuel().has_value());
    ASSERT_FALSE(data_ref.aux_fuel().value());
    data_ref.aux_fuel_data_ref().value().data()[0]->data_ref()->value = 1;
    ASSERT_TRUE(data_ref.aux_fuel().value());
    data_ref.aux_fuel_data_ref().value().data()[0]->data_ref()->value = 0;
    ASSERT_FALSE(data_ref.aux_fuel().value());
    data_ref.aux_fuel_data_ref().value().data()[1]->data_ref()->value = 1;
    ASSERT_TRUE(data_ref.aux_fuel().value());
    data_ref.aux_fuel_data_ref().value().data()[1]->data_ref()->value = 0;
    ASSERT_FALSE(data_ref.aux_fuel().value());
 
    ASSERT_TRUE(data_ref.door_open().has_value());
    ASSERT_FALSE(data_ref.door_open().value());
    data_ref.door_open_data_ref().value().data()[0]->data_ref()->value = 1;
    ASSERT_TRUE(data_ref.door_open().value());
    data_ref.door_open_data_ref().value().data()[0]->data_ref()->value = 0;
    ASSERT_FALSE(data_ref.door_open().value());
    data_ref.door_open_data_ref().value().data()[1]->data_ref()->value = 1;
    ASSERT_TRUE(data_ref.door_open().value());
    data_ref.door_open_data_ref().value().data()[1]->data_ref()->value = 0;
    ASSERT_FALSE(data_ref.door_open().value());
     data_ref.door_open_data_ref().value().data()[2]->data_ref()->value = 1;
    ASSERT_TRUE(data_ref.door_open().value());
    data_ref.door_open_data_ref().value().data()[2]->data_ref()->value = 0;
    ASSERT_FALSE(data_ref.door_open().value());
}

TEST(profile_test, annunciator_min) {
  auto node = YAML::Load(R"(
annunciator:
 master_warn:
  - key: 'sim/cockpit2/annunciators/master_warning'
 oil_low:
  - key: 'sim/cockpit2/annunciators/oil_pressure_low'
 fuel_low:
  - key: 'sim/cockpit2/annunciators/fuel_pressure_low'
 starter:
  - key: 'sim/cockpit2/engine/actuators/starter_hit'
 master_caution:
  - key: 'sim/cockpit2/annunciators/master_caution'
 vacuum_low:
  - key: 'sim/cockpit2/annunciators/low_vacuum'
 volt_low:
  - key: 'sim/cockpit2/annunciators/low_voltage'
    )");


    auto data_ref_opt = annunciator_data_ref::build(node["annunciator"]);
    ASSERT_TRUE(data_ref_opt.has_value());
    auto data_ref = std::move(data_ref_opt.value());

    ASSERT_TRUE(data_ref.master_warn().has_value());
    ASSERT_FALSE(data_ref.master_warn().value());
    data_ref.master_warn_data_ref().value().data().front()->data_ref()->value = 1;
    ASSERT_TRUE(data_ref.master_warn().value());

    ASSERT_TRUE(data_ref.oil_low().has_value());
    ASSERT_FALSE(data_ref.oil_low().value());
    data_ref.oil_low_data_ref().value().data().front()->data_ref()->value = 1;
    ASSERT_TRUE(data_ref.oil_low().value());

    ASSERT_TRUE(data_ref.fuel_low().has_value());
    ASSERT_FALSE(data_ref.fuel_low().value());
    data_ref.fuel_low_data_ref().value().data().front()->data_ref()->value = 1;
    ASSERT_TRUE(data_ref.fuel_low().value());

    ASSERT_TRUE(data_ref.starter().has_value());
    ASSERT_FALSE(data_ref.starter().value());
    data_ref.starter_data_ref().value().data().front()->data_ref()->value = 1;
    ASSERT_TRUE(data_ref.starter().value());

    ASSERT_TRUE(data_ref.master_caution().has_value());
    ASSERT_FALSE(data_ref.master_caution().value());
    data_ref.master_caution_data_ref().value().data().front()->data_ref()->value = 1;
    ASSERT_TRUE(data_ref.master_caution().value());

    ASSERT_TRUE(data_ref.vacuum_low().has_value());
    ASSERT_FALSE(data_ref.vacuum_low().value());
    data_ref.vacuum_low_data_ref().value().data().front()->data_ref()->value = 1;
    ASSERT_TRUE(data_ref.vacuum_low().value());

    ASSERT_TRUE(data_ref.volt_low().has_value());
    ASSERT_FALSE(data_ref.volt_low().value());
    data_ref.volt_low_data_ref().value().data().front()->data_ref()->value = 1;
    ASSERT_TRUE(data_ref.volt_low().value());

    ASSERT_FALSE(data_ref.eng_fire().has_value());
    ASSERT_FALSE(data_ref.anti_ice().has_value());
    ASSERT_FALSE(data_ref.apu().has_value());
    ASSERT_FALSE(data_ref.hydro_low().has_value());
    ASSERT_FALSE(data_ref.parking_brake().has_value());
    ASSERT_FALSE(data_ref.aux_fuel().has_value());
    ASSERT_FALSE(data_ref.door_open().has_value());
}