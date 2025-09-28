#ifndef LOGGER_H_
#define LOGGER_H_

#include <XPLM/XPLMUtilities.h>

#include <sstream>

class logger {
protected:
    std::stringstream output_;

public:
    template<typename T>
    inline
    logger & operator<<(T const & buffer) noexcept {
        output_ << buffer;
        return *this;
    }

    inline
    ~logger() noexcept {
        output_ << std::endl;
        std::string output = "[HCBravo] : " + output_.str();
        XPLMDebugString(output.c_str());
    }
};

#endif