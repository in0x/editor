#pragma once
#include "timer.h" 
#include "core.h"

#ifdef __APPLE__
#include <mach/mach_time.h>
#endif

Timer make_timer()
{
    Timer timer;
#ifdef __APPLE__
    // https://developer.apple.com/documentation/kernel/mach_timebase_info_data_t
    mach_timebase_info_data_t rate_nsec;
    mach_timebase_info(&rate_nsec);
    timer.tick_rate = rate_nsec.numer / rate_nsec.denom;
    timer.last_ticks = mach_absolute_time();
#endif
    return timer;
}

// stamp the timer and return elapsed units since last tick
u64 tick(Timer* timer)
{
    u64 now = 0;
#if defined(__APPLE__)
    now = mach_absolute_time();
#else
    static_assert(false);
#endif
    f64 elapsed = now - timer->last_ticks;
    timer->last_ticks = now;
    return elapsed;
}

// same as tick, but returns elapsed milliseconds
f64 tick_ms(Timer* timer)
{
    u64 elapsed = tick(timer);
    return elapsed / (1'000'000llu * (f64)timer->tick_rate);
}

// same as tick, but returns elapsed seconds
f64 tick_s(Timer* timer)
{
    u64 elapsed = tick(timer);
    return elapsed / (1'000'000'000llu * (f64)timer->tick_rate);
}