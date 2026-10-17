// SPDX-License-Identifier: LGPL-2.1-only
//
// src/logger.h
// XPlane Plugin for HoneyComb Bravo Throttle Controller
//
// Copyright (C) 2005 Isaac Gelado

#ifndef LOGGER_H_
#define LOGGER_H_

#include <sstream>

namespace logger {

// Supplied by the host: X-Plane for the plugin, stderr for offline tools.
void hcbravo_log(const char * message) noexcept;

#if !defined(NDEBUG)
enum class diagnostic_category : unsigned {
    knobs = 1, datarefs = 2, configuration = 4
};

constexpr unsigned diagnostic_bit(diagnostic_category category) noexcept {
    return static_cast<unsigned>(category);
}

// Persistent Debug-only menu settings. All categories start disabled.
class diagnostics {
    inline static unsigned selected_ = 0;
public:
    static unsigned selected() noexcept { return selected_; }
    static bool enabled(diagnostic_category category) noexcept { return (selected_ & diagnostic_bit(category)) != 0; }
    static bool toggle(diagnostic_category category) noexcept {
        selected_ ^= diagnostic_bit(category);
        return enabled(category);
    }
    static void reset() noexcept { selected_ = 0; }
};

// Restrict DataRef tracing to an opted-in knob command.
class diagnostic_scope {
    inline static thread_local unsigned active_ = 0;
    unsigned previous_;

public:
    explicit diagnostic_scope(unsigned categories) noexcept : previous_(active_) { active_ = categories; }
    explicit diagnostic_scope(diagnostic_category category) noexcept : diagnostic_scope(diagnostic_bit(category)) {}
    ~diagnostic_scope() noexcept { active_ = previous_; }
    static bool enabled(diagnostic_category category) noexcept { return (active_ & diagnostic_bit(category)) != 0; }
    diagnostic_scope(const diagnostic_scope &) = delete;
    diagnostic_scope & operator=(const diagnostic_scope &) = delete;
};
#endif

class info {
protected:
    std::stringstream output_;

public:
    template<typename T>
    inline
    info & operator<<(T const & buffer) noexcept {
        output_ << buffer;
        return *this;
    }

    inline
    ~info() noexcept {
        output_ << std::endl;
        std::string output = "[HCBravo] : " + output_.str();
        hcbravo_log(output.c_str());
    }
};

#if !defined(NDEBUG)
using debug = info;
#else
class debug {
public:
    template<typename T>
    debug & operator<<(const T &) noexcept { return *this; }
};
#endif

} // namespace logger

#endif
