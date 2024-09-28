#pragma once

#include "common.h"
#include "table.h"
#include "sampled_animation.h"

struct Triangle_Mesh;
struct Pose_Channel;

// Move this into a animation_player.cpp file later.
struct Animation_Player
{
    RArr<Pose_Channel*> channels;
    
    // @Incomplete: Not using discontinuity info
    // RArr<Discontinuity_Info> discontinuity_info;

    RArr<i32>     mesh_parent_indices; // :( We have to reconstruct this dynamically, but if the info were in the mesh then we could just use it directly.
    Triangle_Mesh *mesh = NULL;

    f64 current_time = 0;
    f64 current_dt   = 0;

    Table<String, i32> node_name_to_index_map;

    RArr<Xform_State> current_states; // These xforms must be in the global space.

    RArr<Matrix4> output_matrices; // These matrices are the skeleton nodes to world transforms.

    u32 eval_count = 0;
    u32 num_changed_channels_since_last_eval = 0;

    bool remove_locomotion      = false; // This is for making an animation become in-place.
    bool needs_hierarchy_update = false;

    RArr<Xform_State> states_relative_to_parent; // Our blended states with respect to the parents from the channels.
};

void set_mesh(Animation_Player *player, Triangle_Mesh *mesh);
Pose_Channel *add_animation_channel(Animation_Player *player);
void accumulate_time(Animation_Player *player, f64 dt);
void eval(Animation_Player *player, bool allow_discontinuity_blending = true);
void reset_animations(Animation_Player *player);

Pose_Channel *get_primary_animation_channel(Animation_Player *player);

Quaternion cmuratori_get_orientation(Matrix4 m);
