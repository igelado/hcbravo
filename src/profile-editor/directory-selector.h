// SPDX-License-Identifier: LGPL-2.1-only
#pragma once

#include <filesystem>
#include <string>
#include <vector>

class profile_editor;

class directory_selector {
    std::string path_;
    std::string browse_path_;
    std::string error_;
    std::filesystem::path browsing_;
    std::vector<std::filesystem::path> folders_;

    bool browse(const std::filesystem::path & directory);
    bool select(profile_editor & editor, const std::filesystem::path & directory);

public:
    explicit directory_selector(const profile_editor & editor);
    void draw(profile_editor & editor);
};
