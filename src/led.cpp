#include "led.h"

#include <cstring>

led_state::led_state() noexcept 
{
    ::memset(reinterpret_cast<void *>(&u), 0, sizeof(u));
}

led_state::led_state(led_state && other) noexcept 
{
    ::memcpy(&u, &other.u, sizeof(u));
}