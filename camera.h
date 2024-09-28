#pragma once

#include "common.h"

struct Camera
{
    Vector3    position;
    Quaternion orientation;

    Vector3    forward_vector;

    Matrix4    world_to_view_matrix;
    Matrix4    view_to_proj_matrix;

    // Not used directly, only used in update_game_camera()
    f32        theta;
    f32        phi;

    // Maybe these should be in a thing called Camera_Control:
    f32        fov_vertical;
    f32        z_near;
    f32        z_far;
};

Matrix4 make_projection_matrix(f32 z_near, f32 z_far, f32 aspect_ratio, f32 fov_vertical);
Matrix4 make_look_at_matrix(Vector3 eye_pos, Vector3 direction, Vector3 up);
void refresh_camera_matrices(Camera *camera);
