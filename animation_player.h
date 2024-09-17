#pragma once

#include "common.h"
#include "table.h"
#include "sampled_animation.h"

struct Triangle_Mesh;
struct Pose_Channel;

struct Combined_State
{
    RArr<Xform_State> xforms; // These xforms must be in the global space.
};

// Move this into a animation_player.cpp file later.
struct Animation_Player
{
    RArr<Pose_Channel*> channels;
    
    // @Incomplete: Not using discontinuity info
    // RArr<Discontinuity_Info> discontinuity_info;

    RArr<i32>     mesh_parent_indices;
    Triangle_Mesh *mesh = NULL;

    f64 current_time = 0;
    f64 current_dt   = 0;

    Table<String, i32> node_name_to_index_map;

    // The combine states are for interpolating between frames and, most importantly, to
    // blend between different animations.
    Combined_State state0;
    Combined_State state1;
    Combined_State *current_state;
    Combined_State *previous_state;

    RArr<Matrix4> output_matrices; // These matrices are the skeleton nodes to world transforms.

    u32 eval_count = 0;
    u32 num_changed_channels_since_last_eval = 0;

    bool remove_locomotion      = false; // This is for making an animation become in-place.
    bool needs_hierarchy_update = false;

    RArr<Matrix4> transforms_relative_to_parent; // Our own copy of the transforms stuff so that we can put the blending transformations on here.
};

void init_player(Animation_Player *player);
void set_mesh(Animation_Player *player, Triangle_Mesh *mesh);
Pose_Channel *add_animation_channel(Animation_Player *player);
void accumulate_time(Animation_Player *player, f64 dt);
void eval(Animation_Player *player, bool allow_discontinuity_blending = true);

Quaternion cmuratori_get_orientation(Matrix4 m);
