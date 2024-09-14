#pragma once

#include "common.h"

#include "entity_manager.h"
#include "level_set.h"

extern bool noclip;

// @Cleanup: Make a campaign state or something:
extern i32 current_level_index;

void advance_level(i32 delta, bool due_to_victory = false);

void init_level_general(bool reset);
void init_sokoban();

Entity_Manager *get_entity_manager();
Guy *get_active_hero(Entity_Manager *manager);

void simulate_sokoban();

void post_undo_reevaluate(Entity_Manager *manager);

Transaction_Id get_next_transaction_id(Entity_Manager *manager);
bool maybe_retire_next_transaction(Entity_Manager *manager);

void enact_next_buffered_move(Entity_Manager *manager, bool first_call = true);

#include "visit_struct.h"
struct Gameplay_Visuals // Strings inside here must not be literals so they must be heap allocated.
{
    f32 visual_position_duration = 1.0f;

    f32 general_turning_degrees_per_second = 500.0f;

    f32 default_running_speed = 5.0f; // How many tiles per second
    f32 move_accel_time_default = 0.15f;
    f32 move_decel_distance_default = 0.15f;

    f32 victory_transition_time = 0.3f;
    f32 level_start_transition_time = 0.6f;

    f32 distance_epsilon = 0.1f;

    f32 wizard_teleport_pre_time = 0.05f;
    f32 wizard_teleport_post_time = 0.33f;
};

// @Cleanup: Because we want the default initialization goodness, we cannot use the intrusive syntax of visit_struct.h
VISITABLE_STRUCT(Gameplay_Visuals,
                 visual_position_duration,
                 general_turning_degrees_per_second,
                 default_running_speed,
                 move_accel_time_default,
                 move_decel_distance_default,
                 victory_transition_time,
                 level_start_transition_time,
                 distance_epsilon,
                 wizard_teleport_pre_time,
                 wizard_teleport_post_time);

extern Gameplay_Visuals gameplay_visuals;
