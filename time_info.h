#pragma once

#include "common.h"

extern f64 global_time_rate;
extern f64 last_time;

struct Time_Info
{
    f32 current_time = 0.0;
    f32 current_dt   = 0.0;

    f32 ui_time      = 0.0;
    f32 ui_dt        = 0.0;

    f32 real_world_time = 0.0;
    f32 real_world_dt   = 0.0;
};

extern Time_Info timez;

// @Note: If init_time() is not called, then the first time get_time() is invoked,
// we will also init the time
void init_time();
f64 get_time(); // Get the total runtime of the application in seconds
void sleep_in_milis(f64 amount);
void update_time(f32 dt_max);
