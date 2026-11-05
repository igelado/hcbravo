// SPDX-License-Identifier: LGPL-2.1-only
#ifndef HCBRAVO_DATAREF_CATALOG_H
#define HCBRAVO_DATAREF_CATALOG_H

#include <stop_token>
#include <string>
#include <unordered_set>

std::unordered_set<std::string> download_dataref_catalog(std::stop_token stop);

#endif
