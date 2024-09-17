#pragma once

#include "common.h"

struct Xform_State
{
    Vector3    translation = Vector3(0, 0, 0);
    Vector3    scale       = Vector3(0, 0, 0);
    Quaternion orientation = Quaternion(1, 0, 0, 0);
};

struct Node_Info
{
    String name;
    
    // Only the root can have parent_index of -1. If anyone
    // else has -1 for their parent's index, that's an error.
    i32 parent_index = -1;
};

struct Keyframe
{
    SArr<Xform_State> xform_states; // Has a count == 'nodes_info'.
};

struct Sampled_Animation
{
    SArr<Node_Info> nodes_info;
    SArr<Keyframe>  keyframes;

    i32     num_samples = 0; // Number of samples per node.
    i32     frame_rate  = 0; // Frames per second.
    f64     duration    = 0; // Duration in second.
    Matrix4 g_matrix;        // Global matrix.
};

bool load_sampled_animation(Sampled_Animation *anim, String full_path);
