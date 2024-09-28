#pragma once

#include "common.h"

struct Key_Info
{
    bool pressed          = false;
    f32  time_accumulated = 0.0f;
    bool signalled        = false;
    bool repeat           = false;
};

extern const f32 TIME_PER_MOVE_REPEAT;
extern Key_Info key_left;
extern Key_Info key_right;
extern Key_Info key_up;
extern Key_Info key_down;
extern Key_Info key_undo;
extern Key_Info key_action;

void read_input();

#include "entity_manager.h"
void update_game_camera(Entity_Manager *manager);

void update_guy_active_flags(Entity_Manager *manager);

Quaternion get_orientation_from_angles(f32 theta, f32 phi, f32 rho = 0);

// These are axis of the world:
extern Vector3 axis_right;
extern Vector3 axis_forward;
extern Vector3 axis_up;
