// SPDX-License-Identifier: LGPL-2.1-only
#include "conf-validation.h"
#include "conf.h"

namespace conf {

std::expected<void, std::string> validate_file(const std::string & path) {
    const auto result = configuration::read(path);
    if(!result) return std::unexpected("invalid configuration (see parser diagnostics)");
    return {};
}

}
