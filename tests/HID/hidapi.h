// SPDX-License-Identifier: LGPL-2.1-only
#pragma once

#include <cstddef>
#include <vector>

struct hid_device_ {};
using hid_device = hid_device_;
namespace hid_test {
inline hid_device device;
inline int result = 65;
inline std::vector<std::vector<unsigned char>> reports;
}
static inline int hid_init() { return 0; }
static inline int hid_exit() { return 0; }
static inline hid_device * hid_open(unsigned short, unsigned short, const wchar_t *) { return &hid_test::device; }
static inline void hid_close(hid_device *) {}
static inline const wchar_t * hid_error(hid_device *) { return L"test HID failure"; }
static inline int hid_send_feature_report(hid_device *, const unsigned char * data, size_t size) {
    hid_test::reports.emplace_back(data, data + size);
    return hid_test::result;
}
