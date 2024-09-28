#include "time_info.h"

// @Todo: Remove time dependency from C++
#include <chrono>

Time_Info timez = {};

f64 global_time_rate = 1.0f;
f64 last_time        = 0.0;
f64 start_time;
bool has_init = false;

f64 time_since_epoch()
{
    auto t = std::chrono::high_resolution_clock::now();
    auto duration_in_seconds_since_epoch = std::chrono::duration<f64>(t.time_since_epoch());
    return duration_in_seconds_since_epoch.count();
}

void init_time()
{
    start_time = time_since_epoch();
    has_init = true;
}

f64 get_time()
{
    if (!has_init) init_time();

    return time_since_epoch() - start_time;
}

void update_time(f32 dt_max)
{
    f64 now = get_time();
    f64 delta = now - last_time;
    f64 dilated_dt = (delta * global_time_rate);

    f32 clamped_dilated_dt = static_cast<f32>(dilated_dt);
    if (clamped_dilated_dt > dt_max) {clamped_dilated_dt = dt_max;}

    timez.current_dt    = clamped_dilated_dt;
    timez.current_time += clamped_dilated_dt;

    timez.real_world_dt    = dilated_dt;
    timez.real_world_time += dilated_dt;

    timez.ui_dt    = static_cast<f32>(delta);
    timez.ui_time += static_cast<f32>(delta);

    last_time = now;
}
