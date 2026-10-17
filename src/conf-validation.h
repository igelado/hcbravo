// SPDX-License-Identifier: LGPL-2.1-only
#ifndef CONF_VALIDATION_H_
#define CONF_VALIDATION_H_

#include <expected>
#include <string>

namespace conf {

// Validates profile structure and values without resolving simulator DataRefs.
std::expected<void, std::string> validate_file(const std::string & path);

}
#endif
