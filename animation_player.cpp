#include "animation_player.h"
#include "mesh_catalog.h"
#include "animation_channel.h"

// constexpr f32 DISCONTINUITY_LERP_TIME = 0.18f;

// struct Discontinuity_Info
// {
//     bool discontinuity = false;
//     f64  time          = 0;
//     f32  lerp_duration = 0;
//     Xform_State state;
// };

void init_player(Animation_Player *player)
{
    player->current_state  = &player->state1;
    player->previous_state = &player->state0;
}

void set_mesh(Animation_Player *player, Triangle_Mesh *mesh)
{
    if (!mesh) return;

    player->needs_hierarchy_update = true;

    table_reset(&player->node_name_to_index_map);

    player->mesh = mesh;

    auto skeleton = mesh->skeleton_info;
    if (!skeleton)
    {
        array_reset(&player->mesh_parent_indices);
    }
    else
    {
        array_reset(&player->mesh_parent_indices);
        array_reset(&player->output_matrices);

        auto nodes_count = skeleton->skeleton_node_info.count;
        // array_resize(&player->mesh_parent_indices, nodes_count);
        // array_resize(&player->output_matrices,     nodes_count);

        i32 it_index = 0;
        for (auto &node : skeleton->skeleton_node_info)
        {
            auto name = node.name;

            table_add(&player->node_name_to_index_map, name, it_index);
            array_add(&player->mesh_parent_indices, -1); // We have to back-fill these dynamically, later in 'update_hierarchy'.

            auto m = node.rest_object_space_to_object_space;
            auto mi = glm::inverse(m);
            array_add(&player->output_matrices, mi);

            it_index += 1;
        }

        assert(player->node_name_to_index_map.count == nodes_count);
    }
}

Pose_Channel *add_animation_channel(Animation_Player *player)
{
    auto channel = New<Pose_Channel>();
    channel->my_aplayer = player;

    array_add(&player->channels, channel);
    player->needs_hierarchy_update = true;

    return channel;
}

void accumulate_time(Animation_Player *player, f64 dt)
{
    for (auto channel : player->channels)
    {
        if (channel->type == Pose_Channel_Type::ANIMATION)
        {
            accumulate_time(channel, dt);
        }

        // IK channels don't really accumulate time...
    }

    player->current_time += dt;
    player->current_dt    = dt;
}

// This update the 'aplayer_index' field in the Node_Info the the given channel based
// on the node_name_to_index_map table inside Animation_Player.
void update_channel_aplayer_indices(Animation_Player *player, Pose_Channel *channel)
{
    for (auto &c_info : channel->nodes_info)
    {
        auto [aplayer_index, found] = table_find(&player->node_name_to_index_map, c_info.name);

        if (!found)
        {
            printf("         *********** What the fuck, couldn't find the aplayer_index for node name '%s'!\n", temp_c_string(c_info.name));
            aplayer_index = -1;
        }

        c_info.aplayer_index = aplayer_index;
    }
}

void update_hierarchy(Animation_Player *player)
{
    //
    // Use the any ANIMATION channel that is active to figure 
    // out the node hierarchy. While we are visiting them, copy
    // their local transform into ours.
    //
    for (auto channel : player->channels)
    {
        update_channel_aplayer_indices(player, channel);

        if (!channel->is_active) continue;
        if (channel->type != Pose_Channel_Type::ANIMATION) continue;

        if (!channel->animation) continue;

        i64 node_index = 0;
        for (auto &c_info : channel->nodes_info)
        {
            auto aplayer_index = c_info.aplayer_index;
            if (aplayer_index == -1) continue;

            auto parent_index_in_animation = channel->animation->nodes_info[node_index].parent_index;
            i32 parent_index_on_aplayer;

            if (parent_index_in_animation == -1)
            {
                parent_index_on_aplayer = -1;
            }
            else
            {
                parent_index_on_aplayer = channel->nodes_info[parent_index_in_animation].aplayer_index;
                assert(parent_index_on_aplayer >= 0);
            }

            player->mesh_parent_indices[aplayer_index] = parent_index_on_aplayer;

            node_index += 1;
        }
    }
}

void make_current_state_be_previous_state(Animation_Player *player)
{
    auto temp = player->previous_state;
    player->previous_state = player->current_state;
    player->current_state  = temp;

    if (player->mesh && player->mesh->skeleton_info)
    {
        array_resize(&player->current_state->xforms, player->mesh->skeleton_info->skeleton_node_info.count);
    }
}

// Got from https://youtu.be/aeDMABSW_KE?si=xUzPJ5-c332tg_aV
Quaternion cmuratori_get_orientation(Matrix4 m)
{
    // @Speed: Fill in the coef's more expediently.
    Vector3 mx(m[0][0], m[1][0], m[2][0]);
    Vector3 my(m[0][1], m[1][1], m[2][1]);
    Vector3 mz(m[0][2], m[1][2], m[2][2]);

    normalize_or_zero(&mx);
    normalize_or_zero(&my);
    normalize_or_zero(&mz);

    f32 coef[3][3];
    coef[0][0] = mx.x;  coef[0][1] = mx.y;  coef[0][2] = mx.z;
    coef[1][0] = my.x;  coef[1][1] = my.y;  coef[1][2] = my.z;
    coef[2][0] = mz.x;  coef[2][1] = mz.y;  coef[2][2] = mz.z;

    Quaternion q;
    f32 trace = coef[0][0] + coef[1][1] + coef[2][2];
    if (trace > 0.0f)
    {
        // |w| > 1/2, may as well choose w > 1/2
        f32 s = sqrtf(trace + 1.0f); // 2w

        q.w = s * .5f;
        s   = .5f / s;      // 1/(4w)

        q.x = (coef[2][1] - coef[1][2]) * s;
        q.y = (coef[0][2] - coef[2][0]) * s;
        q.z = (coef[1][0] - coef[0][1]) * s;
    }
    else
    {
        // |w| <= 1/2
        i32 i = 0;
        if (coef[1][1] > coef[0][0]) i = 1;
        if (coef[2][2] > coef[i][i]) i = 2;

        i32 j = (1 << i) & 3; // i + 1 modulo 3
        i32 k = (1 << j) & 3;

        f32 s = sqrtf(coef[i][i] - coef[j][j] - coef[k][k] + 1.0f);

        q[i] = s * .5f;
        s    = .5f / s;

        q[j] = (coef[i][j] + coef[j][i]) * s;
        q[k] = (coef[k][i] + coef[i][k]) * s;
        q.w  = (coef[k][j] - coef[j][k]) * s;
    }

    return q;
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

void eval(Animation_Player *player, bool allow_discontinuity_blending)
{
    player->num_changed_channels_since_last_eval = 0;

    auto channels = player->channels;
    if (!channels.count) return;

    // if (!channels[0]->is_active) return; // @Hack: Until we have an animation channel 0, don't try to layer anything on top or push output transforms. This is because channel 0 is our main channel.

    if (!channels[0]->states.count) return;

    if (player->needs_hierarchy_update)
    {
        update_hierarchy(player);
        player->needs_hierarchy_update = false;
    }

    //
    // Evaluate channels.
    //
    i64 num_changed = 0; // Number of channels that got changed during evaluations.
    for (auto channel : channels)
    {
        if (!channel->is_active) continue;

        auto changed = eval(channel);
        if (changed) num_changed += 1;
    }

    player->num_changed_channels_since_last_eval = num_changed;

    if (num_changed == 0) return; // Early out if nothing changed.

    make_current_state_be_previous_state(player);

    auto current_state = player->current_state;
    array_resize(&player->transforms_relative_to_parent, current_state->xforms.count);

    //
    // Copy the Animation_Channel transforms data into ours.
    //
    for (auto channel : channels)
    {
        if (channel->type != Pose_Channel_Type::ANIMATION) continue;

        // Assert that we have enough nodes for the transforms data.
        assert(channel->nodes_info.count >= channel->transforms_relative_to_parent.count);

        i64 it_index = 0;
        for (auto &info : channel->nodes_info)
        {
            if (info.aplayer_index == -1) continue;

            assert(info.aplayer_index < player->transforms_relative_to_parent.count);
            if (info.aplayer_index >= player->transforms_relative_to_parent.count) continue;

            assert(it_index < channel->transforms_relative_to_parent.count);
            if (it_index >= channel->transforms_relative_to_parent.count) continue;

            player->transforms_relative_to_parent[info.aplayer_index] = channel->transforms_relative_to_parent[it_index];

            it_index += 1;
        }
    }

    //
    // Concatenate all our local transforms into global ones. Also, extract the states from each
    // matrix so that we can interpolate reasonably.
    //
    // @Speed: We should think about not storing the local transforms as matrices when we
    // stored them during the 'eval' in the Animation_Channel. Rather, we should store them
    // as Xform_States to avoid decomposition here.
    //
    for (i64 i = 0; i < current_state->xforms.count; ++i)
    {
        auto parent_index = player->mesh_parent_indices[i];

        Matrix4 m;
        if (parent_index >= 0)
        {
            assert(parent_index < i);

            Matrix4 parent_state;
            get_matrix(current_state->xforms[parent_index], &parent_state); // We know the parent_state is updated because our parent's index must be less than ours, and we are iterating over this array in order.

            m = parent_state * player->transforms_relative_to_parent[i];
        }
        else
        {
            m = player->transforms_relative_to_parent[i];
        }

        // @Speed: This is the slow part.
        set_from_matrix(&current_state->xforms[i], m);
    }

/* @Incomplete: No discontinuities for now.
    if (player->eval_count && (player->previous_state->xforms.count == current_state->xforms.count))
    {
        // Try to detect new discontinuities.
        i32 discontinuity_count = 0;

        for (auto channel : channels)
        {
            if (channel->discontinuity) discontinuity_count += 1;
            channel->discontinuity = false;
        }

        // If necessary, detect and mark new discontinuities.
        if (discontinuity_count && allow_discontinuity_blending)
        {
            for (i64 i = 0; i < current_state->xforms.count; ++i)
            {
                auto discont = &discontinuity_info[i];

                // Set the discontinuity data even if we don't declare a discontinuity here...
                // We will nedd it if this is someone else's parent.
                discont->state_lite    = player->previous_state->xforms[i];
                discont->time          = player->current_time;
                discont->lerp_duration = DISCONTINUITY_LERP_TIME;
                discont->discontinuity = true;
            }
        }
    }

    interpolate_discontinuities(player);

    // @Incomplete: No remove_locomotions for now.
    if (current_state->xforms.count && player->remove_locomotion)
    {
        // Clear locomotion from the animation player.
        auto root_translation = current_state->xforms[0].translation;
        Vector3 translation_correction = -root_translation;

        // This @Hack makes the root bone positioned at the projection point of the pelvis
        // on the ground plane (because most of the game logic happens at the feet of the mesh).
        translation_correction.z = 0.f;

        for (i64 i = 0; i < current_state->xforms.count; ++i)
        {
            auto state = &current_state->xforms[i];
            state->translation += translation_correction;
        }
    }
*/

    array_resize(&player->output_matrices, current_state->xforms.count);

    // @Speed: Fill in the output transforms....
    for (i64 i = 0; i < current_state->xforms.count; ++i)
    {
        get_matrix(current_state->xforms[i], &player->output_matrices[i]);
    }

    player->eval_count += 1;
}
