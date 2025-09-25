#include "led.h"

led_state::led_state() noexcept :
    dirty_(false)
{
    ::memset(reinterpret_cast<void *>(&u), 0, sizeof(u));
}

led_state::led_state(led_state && other) noexcept :
    dirty_(other.dirty_.load(std::memory_order_acquire))
{
    ::memcpy(&u, &other.u, sizeof(u));
}