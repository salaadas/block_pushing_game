#pragma once

#include "common.h"
#include "sampled_animation.h"

struct Animation_Player;

enum class Pose_Channel_Type
{
    UNINITIALIZED = 0,
    IK, // @Incomplete: Not doing IK.
    ANIMATION
};

struct Pose_Channel
{
    Pose_Channel_Type type = Pose_Channel_Type::UNINITIALIZED;

    // These nodes get mapped to the indices in the animation player.
    struct Node_Info
    {
        String name;
        // For every node in the animation channel, this maps to the index of
        // the node inside the Triangle_Mesh, which is retrieved by looking up
        // the 'node_name_to_index_map' in the Animation_Player.
        // We are doing this because not every node in the animation has the
        // same index as the nodes inside the mesh file.
        //
        // For embedded animations, this won't matter, but for animations living
        // in seperate files and the mesh in another file, this is the way to
        // connect the nodes' indices together from 2 different files.
        i32 aplayer_index = 0;
    };

    RArr<Node_Info>   nodes_info;
    RArr<Xform_State> states_relative_to_parent; // Xform_States could be stored within Node_Info but it is convenient to have these packed.

    Animation_Player *my_aplayer = NULL;

    bool is_active     = true;
    bool discontinuity = false;

    //
    // These member are for ANIMATION channel type:
    //

    // These are serialized:
    f32  time_multiplier = 1.0f;
    bool should_loop     = true;
    bool completed       = false; // This gets passed up to the animation player.
    bool completed_state_propagated_through = false;
    f64  current_time       = 0;
    f64  animation_duration = 0; // Because we may unload the animation or just not have it loaded.
    f32  end_time           = 0; // Maybe shorter than the duration because this allows for playing a fraction of the animation.

    // These are not serialized:
    Sampled_Animation *animation = NULL;
    i32 issue_number = 0;
    bool evaluated_at_least_once = false;

    f32 blend_out_t        = -1;
    f32 blend_out_duration = -1;
//    Sampled_Animation *transition_into = NULL; // @Hack: This is just to get the animation blending stuff going, we would probably want to determine transitions from the Animation_Graph and Arc.

    f32 blend_factor_for_debug_output = 1.0f; // This is only being used in animation_hud.cpp for showing how much this channel is being blended in the Animation_Player.
};

// constexpr f64 TOO_SHORT_TO_COUNT_AS_MOTION = 1.0; // Will have to get smaller!

void set_speed(Pose_Channel *channel, f32 time_multiplier);
void set_animation(Pose_Channel *channel, Sampled_Animation *sanim, f64 start_time);
bool eval(Pose_Channel *channel);
void accumulate_time(Pose_Channel *channel, f64 dt);

void set_from_matrix(Xform_State *state, Matrix4 m); // @Speed:
void get_matrix(Xform_State xform, Matrix4 *m); // @Speed:
void get_inverse_matrix(Xform_State xform, Matrix4 *result); // @Speed:

Xform_State lerp(Xform_State xform0, Xform_State xform1, f64 t);
