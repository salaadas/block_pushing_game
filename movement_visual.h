#pragma once

#include "common.h"

#include "visit_struct.h"

// @ForwardDeclare
typedef u32 Pid;
typedef u32 Transaction_Id;

enum class Linear_Move_Type
{
    NONE   = 0,
    PUSHED = 1,
    PULLED = 2,
};

enum class Move_Type
{
    LINEAR     = 0,
    TELEPORT   = 1,
    STATIONARY = 2,
};

struct Visual_Interpolation
{
    Move_Type move_type;

    Linear_Move_Type linear_move_type;
    // Linear_Move_Type just_completed_linear_move_type;

    Vector3 visual_start;
    Vector3 visual_end;

    f32 start_time = -1.0;
    f32 duration   = 0;
    f32 elapsed    = 0;

    Transaction_Id transaction_id = 0;

    // bool is_falling;
    Vector3 teleport_direction;

    Pid sync_id = 0;

    Vector3 velocity;
    Vector3 old_velocity;

    // f32 speed_scale; // 1.0

    f32 teleport_pre_time;
    f32 teleport_post_time;
    bool has_teleported = false;

    // Pid moving_onto_id;

    // bool caused_by_timed_event; // false
    bool done = false;
    // bool strong_push; // false

    // i32 transaction_hold_z_index;
};

VISITABLE_STRUCT(Visual_Interpolation,
                 move_type,
                 linear_move_type,
                 visual_start, visual_end,
                 start_time, duration, elapsed,
                 transaction_id, sync_id,
                 velocity,
                 teleport_direction, has_teleported,
                 done);

struct Entity;
struct Entity_Manager;

#include "source_location.h"

void reset(Visual_Interpolation *v, Source_Location loc = Source_Location::current());
void add_visual_interpolation(Entity *e, Vector3 old_position, Vector3 new_position, Move_Type move_type, f32 duration,
                              Transaction_Id transaction_id, Source_Location loc = Source_Location::current());
void update_entities_visual_interpolation(Entity_Manager *manager, f32 dt);
