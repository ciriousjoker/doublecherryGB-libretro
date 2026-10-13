#pragma once
#include <cstdint>
#include <limits>

// Relative deadlines and observation intervals must be below half the 32-bit range.
inline bool clock_after(uint32_t now, uint32_t deadline) {
    const uint32_t delta = now - deadline;
    return delta != 0 && delta < 0x80000000u;
}
inline bool clock_at_or_after(uint32_t now, uint32_t deadline) {
    return uint32_t(now - deadline) < 0x80000000u;
}
// Keep inactivity separate: zero is a legitimate deadline after rollover.
inline bool clock_waiting(uint32_t now, uint32_t deadline, bool &active) {
    if (active && clock_after(now, deadline)) active = false;
    return active;
}
inline int64_t clock_signed_delta(uint32_t now, uint32_t before) {
    const uint32_t delta = now - before;
    return delta < 0x80000000u ? int64_t(delta) : -int64_t(0x100000000ull - delta);
}
inline int clock_pulse_duration(uint32_t now, uint32_t before, int adjustment = 0) {
    const int64_t value = clock_signed_delta(now, before) + adjustment;
    return value <= 0 ? 0 : value > std::numeric_limits<int>::max()
        ? std::numeric_limits<int>::max() : int(value);
}
static_assert(sizeof(int) == sizeof(uint32_t), "Legacy clocks require 32-bit int fields");
