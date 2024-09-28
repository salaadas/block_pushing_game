#include "animation_channel.h"
#include "animation_player.h"

constexpr bool DISABLE_LERPS = false;

// 'start_time' doesn't need to be 0, it just indicates when do we want to start playing the animation.
void set_animation(Pose_Channel *channel, Sampled_Animation *sanim, f64 start_time)
{
    channel->type = Pose_Channel_Type::ANIMATION;

    if (sanim)
    {
        // @Robustness: Log here in case some wacky stuff happens.
        if (!sanim->num_samples)      sanim = NULL;
        if (!sanim->nodes_info.count) sanim = NULL;
    }

    if ((!sanim) && (!channel->animation)) return;
    
    channel->issue_number += 1;

    channel->animation = sanim;
    if (channel->animation)
    {
        channel->animation_duration = sanim->duration;
    }
    else
    {
        channel->animation_duration = 0;
    }

    channel->end_time        = channel->animation_duration;
    channel->time_multiplier = 1;
    channel->discontinuity   = false;
    channel->completed       = false;
    channel->completed_state_propagated_through = false;

    array_reset(&channel->nodes_info);
    if (!sanim)
    {
        array_reset(&channel->states_relative_to_parent);
    }
    else
    {
        for (auto &a_node : sanim->nodes_info)
        {
            Pose_Channel::Node_Info info;
            info.name = a_node.name;
            array_add(&channel->nodes_info, info);
        }

        array_resize(&channel->states_relative_to_parent, sanim->nodes_info.count);
    }

    assert(channel->my_aplayer);
    channel->my_aplayer->needs_hierarchy_update = true; // Since we just added 0s in the nodes_info array of the channel, we will need to recompute these later, which is what update_hierarchy() does.
 
    channel->is_active    = true;
    channel->current_time = start_time;
}

i64 get_animation_node_index(Pose_Channel *channel, String name) // Not used rn.
{
    if (channel->animation)
    {
        i64 it_index = 0;
        for (auto &a_info : channel->animation->nodes_info)
        {
            if (a_info.name == name) return it_index;
            it_index += 1;
        }
    }

    return -1;
}

i64 get_node_index_of_channel(Pose_Channel *channel, String name) // Not used rn.
{
    i64 it_index = 0;
    for (auto &info : channel->nodes_info)
    {
        if (info.name == name) return it_index;
        it_index += 1;
    }

    return -1;
}

void set_speed(Pose_Channel *channel, f32 multiplier)
{
    channel->time_multiplier = multiplier;
}

// 'fraction' here means a floating point fraction of the duration between 0 and 1.
void set_time_by_fraction(Pose_Channel *channel, f32 fraction)
{
    assert(fraction >= 0.0f);
    assert(fraction <= 1.0f);

    f64 time = channel->animation_duration * fraction;
    channel->current_time = time;
}

void accumulate_time(Pose_Channel *channel, f64 dt)
{
    auto old_time = channel->current_time;
    channel->current_time += dt * channel->time_multiplier;

    // If it went past the end.
    if ((channel->current_time != old_time) && (channel->current_time >= channel->end_time))
    {
        if (!channel->should_loop)
        {
            channel->current_time = channel->end_time;

            if (!channel->completed)
            {
                channel->completed = true;
                channel->completed_state_propagated_through = false;
            }

            return;
        }

        // @Note: We are not subtracting by the channel's animation duration because, the end time
        // could be shorter than the whole duration, so we only seek to the beginning.
        channel->current_time -= channel->end_time;
    }
}

inline
Xform_State lerp(Xform_State xform0, Xform_State xform1, f64 t)
{
    Xform_State result;

    result.translation = lerp(xform0.translation, xform1.translation, t);
    // If dot product of two interpolating quaternions is < 0, it'll take the long route
    // around the sphere. To fix this, negate one of the quaternions.
    if (glm::dot(xform0.orientation, xform1.orientation) < 0)
    {
        result.orientation = nlerp(xform0.orientation, negate(xform1.orientation), t);
    }
    else
    {
        result.orientation = nlerp(xform0.orientation, xform1.orientation, t);
    }

    result.scale = lerp(xform0.scale, xform1.scale, t);

    return result;
}

// This gets the xform states for all the skeleton nodes at a time in point.
void get_xform_state(Sampled_Animation *anim, f64 time, RArr<Xform_State> *states, i64 max_node_index = -1) // @Cleanup: Are we using the 'max_node_index' ?
{
    auto nodes_count = anim->nodes_info.count;
    if (!nodes_count) return;

    i32 sample_index = (anim->num_samples - 1) * (time / anim->duration);
    Clamp(&sample_index, 0, anim->num_samples - 1);

    f64 fraction = 0; // How far into the current frame are we.
    if (anim->num_samples > 1)
    {
        auto denom = static_cast<f64>(anim->num_samples - 1);

        auto time_per_sample = anim->duration * 1.0f         / denom;
        auto time_base       = anim->duration * sample_index / denom;

        fraction = (time - time_base) / time_per_sample;
    }

    Clamp(&fraction, 0.0, 1.0);

    if (max_node_index == -1)               max_node_index = nodes_count - 1;
    if (max_node_index > (nodes_count - 1)) max_node_index = nodes_count - 1;

    auto frame = &anim->keyframes[sample_index];
    for (i64 i = 0; i <= max_node_index; ++i)
    {
        auto node = &anim->nodes_info[i];

        if (DISABLE_LERPS || (sample_index == (anim->num_samples - 1)))
        {
            (*states)[i] = frame->xform_states[i];
        }
        else
        {
            // Because of the way we chose our sample index, we know that
            // 'sample_index + 1' will never go over the count of the keyframes array.
            auto next_frame = &anim->keyframes[sample_index + 1];

            auto xform0  = frame->xform_states[i];
            auto xform1  = next_frame->xform_states[i];
            (*states)[i] = lerp(xform0, xform1, fraction);
        }
    }
}

void get_matrix(Xform_State xform, Matrix4 *m) // @Speed:
{
    set_rotation(m, xform.orientation);

    // First row.
    (*m)[0][0] *= xform.scale.x;
    (*m)[1][0] *= xform.scale.x;
    (*m)[2][0] *= xform.scale.x;

    // Second row.
    (*m)[0][1] *= xform.scale.y;
    (*m)[1][1] *= xform.scale.y;
    (*m)[2][1] *= xform.scale.y;

    // Third row.
    (*m)[0][2] *= xform.scale.z;
    (*m)[1][2] *= xform.scale.z;
    (*m)[2][2] *= xform.scale.z;

    (*m)[3][0] = xform.translation.x;
    (*m)[3][1] = xform.translation.y;
    (*m)[3][2] = xform.translation.z;

    (*m)[0][3] = 0;
    (*m)[1][3] = 0;
    (*m)[2][3] = 0;
    (*m)[3][3] = 1;
}

void get_inverse_matrix(Xform_State state, Matrix4 *result) // @Speed:
{
    auto t = Matrix4(1.0f);
    t = glm::translate(t, -1.0f * state.translation);

    Matrix4 r;
    set_rotation(&r, glm::conjugate(state.orientation));

    // I bet I don't pass a zero scale in here....
    assert(state.scale.x && state.scale.y && state.scale.z);

    f32 ix = 1.0f / state.scale.x;
    f32 iy = 1.0f / state.scale.y;
    f32 iz = 1.0f / state.scale.z;

    // First row.
    r[0][0] *= ix;
    r[1][0] *= ix;
    r[2][0] *= ix;

    // Second row.
    r[0][1] *= iy;
    r[1][1] *= iy;
    r[2][1] *= iy;

    // Third row.
    r[0][2] *= iz;
    r[1][2] *= iz;
    r[2][2] *= iz;

    // Fourth row.
    r[0][3] = 0;
    r[1][3] = 0;
    r[2][3] = 0;
    r[3][3] = 1;

    // Fourth column.
    r[3][0] = 0;
    r[3][1] = 0;
    r[3][2] = 0;

    *result = r * t;
}

void set_from_matrix(Xform_State *state, Matrix4 m) // @Speed:
{
    Vector3 mx(m[0][0], m[1][0], m[2][0]); // First row.
    Vector3 my(m[0][1], m[1][1], m[2][1]); // Second row.
    Vector3 mz(m[0][2], m[1][2], m[2][2]); // Third row.

    state->scale = Vector3(glm::length(mx), glm::length(my), glm::length(mz));
    state->translation = Vector3(m[3][0], m[3][1], m[3][2]);

    state->orientation = cmuratori_get_orientation(m); // @Cleanup: Why use this instead of the below?
    // state->orientation = glm::quat_cast(m);
}

// 'eval' takes whatever the current time of the channel is, and seeks
// into the animation by that amount. Then, you get the time stamp
// between the two samples and then interpolate between them.
bool eval(Pose_Channel *channel)
{
    if (!channel->animation)
    {
        if (!channel->nodes_info.count) return true; // No change!

        array_reset(&channel->nodes_info);
        return false; // Didn't evaluate anything because we changed from some animation data to no animation data.
    }

    if (channel->completed && channel->completed_state_propagated_through) return false; // Early out.

    assert(channel->states_relative_to_parent.count == channel->animation->nodes_info.count);
    get_xform_state(channel->animation, channel->current_time, &channel->states_relative_to_parent);

    //
    // Convert to local space so we can blend later. However, GLTF format already stores
    // the transforms in local space relative to the parent of the node, we don't need to do anything?
    //
    assert(channel->nodes_info.count == channel->states_relative_to_parent.count);

    // @Speed: We need operator * for Xform_State because this is slow.
    for (i64 i = 0; i < channel->states_relative_to_parent.count; ++i)
    {
        auto a_info = &channel->animation->nodes_info[i];
        if (a_info->parent_index < 0)
        {
            Matrix4 m;
            get_matrix(channel->states_relative_to_parent[i], &m);

            // In case our 'root' isn't the actual root in the modeling software,
            // we account for that by using the g_matrix.
            m = channel->animation->g_matrix * m;

            Xform_State root;
            set_from_matrix(&root, m);
            channel->states_relative_to_parent[i] = root;

            break;
        }
    }

/*
    array_reset(&channel->transforms_relative_to_parent);

    // @Note: When we loaded the sampled animation, GLTF already stores the animation
    // transformations in the samplers with respect the to the rest object space (bind pose)
    // of the skeleton nodes. Because of this, we can just add up the transformations
    // with respect to the parents.
    for (i64 i = 0; i < channel->states.count; ++i)
    {
        // The transformations matrices are relative to the local space.
        Matrix4 m;
        get_matrix(channel->states[i], &m);

        auto a_info = &channel->animation->nodes_info[i];
        if (a_info->parent_index >= 0)
        {
            auto parent_index = a_info->parent_index;
            assert(parent_index < i);

            // We can do this because I think GLTF contains node
            // transforms relative to parent already.
            auto relative = m;
            array_add(&channel->transforms_relative_to_parent, relative);
        }
        else
        {
            // In case our 'root' isn't the actual root in the modeling software,
            // we account for that by using the g_matrix.
            m = channel->animation->g_matrix * m;
            array_add(&channel->transforms_relative_to_parent, m);
        }
    }
*/

    if (channel->nodes_info.count) channel->evaluated_at_least_once = true;
    if (channel->completed) channel->completed_state_propagated_through = true; // Is this branch dead?

    return true;
}
