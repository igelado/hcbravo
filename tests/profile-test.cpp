#include <gtest/gtest.h>

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