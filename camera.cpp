#include "camera.h"

Matrix4 make_perspective_matrix(f32 z_near, f32 z_far, f32 aspect_ratio, f32 fov_vertical)
{
    const f32 DEG2RAD = acos(-1.0f) / 180.0f;
    f32 tangent = tan(fov_vertical/2.0f * DEG2RAD); // Tangent of half fov vertical
    f32 top     = z_near * tangent;                 // Half height of near plane
    f32 right   = top * aspect_ratio;               // Half width of near plane

    Matrix4 my_own_perspective = Matrix4(1.0f);
    my_own_perspective[0][0]   = z_near / right;
    my_own_perspective[1][1]   = z_near / top;
    my_own_perspective[2][2]   = -(z_far + z_near) / (z_far - z_near);
    my_own_perspective[2][3]   = -1;
    my_own_perspective[3][2]   = -(2 * z_far * z_near) / (z_far - z_near);
    my_own_perspective[3][3]   = 0;

    return my_own_perspective;
}

Matrix4 make_look_at_matrix(Vector3 eye_pos, Vector3 _direction)
{
    //
    // Our game uses the right-handed coordinate system with x-right, y-forward. This applies to 
    // the models and also the camera positions. However, opengl expects a z-backward, x-right.
    // Because of this, we need to swap around the axis for the rotation of the basis.
    //

    Matrix4 M = Matrix4(1.0f);

    auto direction = _direction;
    normalize_or_zero(&direction);
    
    Vector3 up = Vector3(0, 0, 1);

    Vector3 right = glm::cross(_direction, up);
    normalize_or_zero(&right);

    up = glm::cross(right, direction);
    normalize_or_zero(&up);

    M[0][0] = right.x;
    M[1][0] = right.y;
    M[2][0] = right.z;

    M[0][1] = up.x;
    M[1][1] = up.y;
    M[2][1] = up.z;

    M[0][2] = -direction.x;
    M[1][2] = -direction.y;
    M[2][2] = -direction.z;

    M[3][0] = -glm::dot(right, eye_pos);
    M[3][1] = -glm::dot(up, eye_pos);
    M[3][2] =  glm::dot(direction, eye_pos);

    return M;
}

#include "opengl.h"
#include "player_control.h"
void refresh_camera_matrices(Camera *camera)
{
    f32 aspect = static_cast<f32>(render_target_width) / render_target_height;
    camera->view_to_proj_matrix = make_perspective_matrix(camera->z_near, camera->z_far,
                                                          aspect, camera->fov_vertical);

    camera->world_to_view_matrix = make_look_at_matrix(camera->position, camera->forward);

    // Refresh the camera orientation quaternion.
    camera->orientation = get_orientation_from_angles(camera->theta, camera->phi);
}

