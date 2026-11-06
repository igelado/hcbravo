// SPDX-License-Identifier: LGPL-2.1-only
#include <gtest/gtest.h>
#include "directory-paths.h"

TEST(DirectoryPaths, DefaultsToWorkingDirectory) {
    EXPECT_EQ(initial_configuration_directory(), std::filesystem::current_path());
}

TEST(DirectoryPaths, MissingInitialDirectoryFallsBackToWorkingDirectory) {
    const auto missing = std::filesystem::current_path() / "hcbravo-nonexistent-directory";
    ASSERT_FALSE(std::filesystem::exists(missing));
    EXPECT_EQ(initial_configuration_directory(missing), std::filesystem::current_path());
}

TEST(DirectoryPaths, ExistingInitialDirectoryIsResolved) {
    EXPECT_EQ(initial_configuration_directory("."), std::filesystem::canonical("."));
}

TEST(DirectoryPaths, RelativeNavigationUsesDisplayedDirectory) {
    const auto current = std::filesystem::canonical(std::filesystem::temp_directory_path());
    EXPECT_EQ(std::filesystem::canonical(resolve_directory_path("..", current)),
              std::filesystem::canonical(current.parent_path()));
    EXPECT_EQ(resolve_directory_path("child/../..", current), current / "child/../..");
    EXPECT_EQ(resolve_directory_path(current, std::filesystem::current_path()), current);
    EXPECT_TRUE(resolve_directory_path({}, current).empty());
}
