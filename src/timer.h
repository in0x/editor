#pragma once
#include "core.h"

struct Timer
{
    u64 last_ticks = 0;
    u64 tick_rate = 0;
};

Timer make_timer();

// stamp the timer and return elapsed units since last tick
u64 tick(Timer* timer);

// same as tick, but returns elapsed milliseconds
f64 tick_ms(Timer* timer);

// same as tick, but returns elapsed seconds
f64 tick_s(Timer* timer);