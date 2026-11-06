// SPDX-License-Identifier: LGPL-2.1-only
#pragma once

#include <filesystem>

inline std::filesystem::path initial_configuration_directory(const std::filesystem::path & requested = {}) {
    std::error_code error;
    if(!requested.empty() && std::filesystem::is_directory(requested, error)) {
        const auto resolved = std::filesystem::canonical(requested, error);
        if(!error) return resolved;
    }
    return std::filesystem::current_path();
}

inline std::filesystem::path resolve_directory_path(const std::filesystem::path & requested,
                                                   const std::filesystem::path & current) {
    if(requested.empty()) return {};
    return requested.is_absolute() ? requested : current / requested;
}
