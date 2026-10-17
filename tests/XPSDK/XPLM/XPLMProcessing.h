// SPDX-License-Identifier: LGPL-2.1-only
#pragma once

using XPLMFlightLoopID = void *;
using XPLMFlightLoop_f = float (*)(float, float, int, void *);
inline constexpr int xplm_FlightLoop_Phase_BeforeFlightModel = 0;
struct XPLMCreateFlightLoop_t {
    int structSize;
    int phase;
    XPLMFlightLoop_f callbackFunc;
    void * refcon;
};

namespace xplm_test {
inline XPLMCreateFlightLoop_t flight_loop {};
inline float scheduled_interval = 0;
inline int schedule_count = 0;
inline float tick() { return flight_loop.callbackFunc(0, 0, 0, flight_loop.refcon); }
}

static inline XPLMFlightLoopID XPLMCreateFlightLoop(XPLMCreateFlightLoop_t * params) {
    xplm_test::flight_loop = *params;
    return &xplm_test::flight_loop;
}
static inline void XPLMDestroyFlightLoop(XPLMFlightLoopID) { xplm_test::flight_loop = {}; }
static inline void XPLMScheduleFlightLoop(XPLMFlightLoopID, float interval, int) {
    xplm_test::scheduled_interval = interval;
    ++xplm_test::schedule_count;
}
