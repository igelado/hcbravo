// SPDX-License-Identifier: LGPL-2.1-only
#include <gtest/gtest.h>
#include "profile-editor.h"

#include <chrono>
#include <fstream>

namespace {
struct directory_fixture {
    std::filesystem::path root = std::filesystem::temp_directory_path() /
        ("hcbravo-editor-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::path first = root / "first", second = root / "second";
    directory_fixture() {
        std::filesystem::create_directories(first);
        std::filesystem::create_directory(second);
        std::ofstream(second / "profile.yaml") <<
            "name: Test\naircrafts: [Test]\nmodels: [TEST]\nsystem: {volts: sim/test/volts}\n";
    }
    ~directory_fixture() {
        std::error_code ignored;
        std::filesystem::remove(second / "profile.yaml", ignored);
        std::filesystem::remove(first, ignored);
        std::filesystem::remove(second, ignored);
        std::filesystem::remove(root, ignored);
    }
};
}

TEST(ProfileEditorDirectory, SwitchesToSelectedDirectoryAndAcceptsEmptyDirectories) {
    directory_fixture paths;
    profile_editor editor(paths.first);
    EXPECT_NO_THROW(editor.set_directory(paths.second));
    EXPECT_EQ(editor.directory(), std::filesystem::canonical(paths.second));
    EXPECT_NO_THROW(editor.set_directory(paths.first));
    EXPECT_EQ(editor.directory(), std::filesystem::canonical(paths.first));
}

TEST(ProfileEditorDirectory, InvalidSelectionKeepsCurrentDirectory) {
    directory_fixture paths;
    profile_editor editor(paths.first);
    EXPECT_THROW(editor.set_directory(paths.root / "missing"), std::exception);
    EXPECT_EQ(editor.directory(), paths.first);
    EXPECT_THROW(editor.set_directory(paths.second / "profile.yaml"), std::exception);
    EXPECT_EQ(editor.directory(), paths.first);
    EXPECT_THROW(editor.set_directory({}), std::exception);
    EXPECT_EQ(editor.directory(), paths.first);
}
