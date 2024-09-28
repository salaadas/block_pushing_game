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
        array_resize(&player->mesh_parent_indices, nodes_count);
        array_resize(&player->output_matrices,     nodes_count);

        auto m = Matrix4(1.0f);

        i32 it_index = 0;
        for (auto &node : skeleton->skeleton_node_info)
        {
            auto name = node.name;

            // printf("-- Adding a mapping from '%s' -> %d\n", temp_c_string(node.name), it_index);

            table_add(&player->node_name_to_index_map, name, it_index);
            player->mesh_parent_indices[it_index] = -1; // We have to back-fill these dynamically, later in 'update_hierarchy'.
            player->output_matrices[it_index] = m;

            it_index += 1;
        }

        assert(player->node_name_to_index_map.count == nodes_count);
    }
}

void reset_animations(Animation_Player *player)
{
    for (auto it : player->channels)
    {
        my_free(it);
    }

    array_reset(&player->channels);
}

Pose_Channel *add_animation_channel(Animation_Player *player)
{
    auto channel = New<Pose_Channel>();
    channel->my_aplayer = player;
    channel->type = Pose_Channel_Type::ANIMATION;

    array_add(&player->channels, channel);
    player->needs_hierarchy_update = true;

    return channel;
}

void accumulate_time(Animation_Player *player, f64 dt)
{
    // Explicit iteration so that we ordered-remove.
    i64 target = 0;
    for (i64 i = 0; i < player->channels.count; ++i)
    {
        auto channel = player->channels[i];

        // IK channels don't really accumulate time...
        if (channel->type == Pose_Channel_Type::ANIMATION)
        {
            accumulate_time(channel, dt);
            if (channel->blend_out_t >= 0)
            {
                channel->blend_out_t += dt;
                if (channel->blend_out_t >= channel->blend_out_duration)
                {
                    // Ordered remove...
                    my_free(channel);
                    continue;
                }
            }
        }

        if (target != i) player->channels[target] = channel;
        target += 1;
    }

    auto num_removed = player->channels.count - target;
    if (num_removed)
    {
        player->channels.count -= num_removed;
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

// Got from https://youtu.be/aeDMABSW_KE?si=xUzPJ5-c332tg_aV
Quaternion cmuratori_get_orientation(Matrix4 m)
{
    // @Speed: Fill in the coef's more expediently.
    Vector3 mx(m[0][0], m[1][0], m[2][0]);
    Vector3 my(m[0][1], m[1][1], m[2][1]);
    Vector3 mz(m[0][2], m[1][2], m[2][2]);

    normalize_or_zero(&mx); // Normalize because we rotate around unit vectors.
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

void eval(Animation_Player *player, bool allow_discontinuity_blending)
{
    player->num_changed_channels_since_last_eval = 0;

    auto channels = player->channels;
    if (!channels.count) return;

    // if (!channels[0]->is_active) return; // @Hack: Until we have an animation channel 0, don't try to layer anything on top or push output transforms. This is because channel 0 is our main channel.

    if (!channels[0]->states_relative_to_parent.count) return;

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

    if (player->mesh && player->mesh->skeleton_info)
    {
        array_resize(&player->current_states, player->mesh->skeleton_info->skeleton_node_info.count);
    }

    array_resize(&player->states_relative_to_parent, player->current_states.count);

    //
    // Copy the Pose_Channel transforms data into ours.
    //
    auto first = true;
    auto last_blend_factor = 1.0f;
    // auto last_blend_name = String("(none)"); // For debugging.

    for (auto channel : channels)
    {
        if (channel->type != Pose_Channel_Type::ANIMATION) continue;

        // Assert that we have enough nodes for the transforms data.
        assert(channel->nodes_info.count >= channel->states_relative_to_parent.count);

        i64 it_index = 0;
        for (auto &info : channel->nodes_info)
        {
            auto a_index = info.aplayer_index;

            if (a_index == -1) continue;

            assert(a_index < player->states_relative_to_parent.count);
            if (a_index >= player->states_relative_to_parent.count) continue;

            assert(it_index < channel->states_relative_to_parent.count);
            if (it_index >= channel->states_relative_to_parent.count) continue;

            if (first)
            {
                player->states_relative_to_parent[a_index] = channel->states_relative_to_parent[it_index];
            }
            else
            {
                if (last_blend_factor != 1)
                {
                    auto lerped = lerp(player->states_relative_to_parent[a_index], channel->states_relative_to_parent[it_index], last_blend_factor);

                    player->states_relative_to_parent[a_index] = lerped;
                }
                else
                {
                    player->states_relative_to_parent[a_index] = channel->states_relative_to_parent[it_index];
                }

                // last_blend_name = channel->animation->name; // In case we want it for debugging.
            }

            it_index += 1;
        }

        if (channel->blend_out_t < 0)
        {
            last_blend_factor = 1.0f;
            channel->blend_factor_for_debug_output = 1; // Special case.
        }
        else
        {
            last_blend_factor = channel->blend_out_t / channel->blend_out_duration;
            Clamp(&last_blend_factor, 0.0f, 1.0f);
            channel->blend_factor_for_debug_output = 1 - last_blend_factor;
        }

        first = false;
    }

    //
    // Concatenate all our local transforms into global ones. Also, extract the states from each
    // matrix so that we can interpolate reasonably.
    //
    for (i64 i = 0; i < player->current_states.count; ++i)
    {
        // @Speed: We should instead make a * operator for Xform State.

        Matrix4 my_state;
        get_matrix(player->states_relative_to_parent[i], &my_state);

        Matrix4 m;
        auto parent_index = player->mesh_parent_indices[i];
        if (parent_index >= 0)
        {
            assert(parent_index < i);

            Matrix4 parent_state;
            get_matrix(player->current_states[parent_index], &parent_state); // We know the parent_state is updated because our parent's index must be less than ours, and we are iterating over this array in order.

            m = parent_state * my_state;
        }
        else
        {
            m = my_state;
        }

        set_from_matrix(&player->current_states[i], m);
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
*/

    if (player->current_states.count && player->remove_locomotion)
    {
        // Clear locomotion from the animation player.
        auto root_translation          = player->current_states[0].translation;
        Vector3 translation_correction = -root_translation;

        // This @Hack makes the root bone positioned at the projection point of the pelvis
        // on the ground plane (because most of the game logic happens at the feet of the mesh).
        translation_correction.z = 0.f;

        for (i64 i = 0; i < player->current_states.count; ++i)
        {
            auto state = &player->current_states[i];
            state->translation += translation_correction;
        }
    }

    auto mesh = player->mesh;
    assert(mesh);

    // @Speed: Fill in the output transforms....
    for (i64 i = 0; i < player->current_states.count; ++i)
    {
        Matrix4 m;
        get_matrix(player->current_states[i], &m);
        player->output_matrices[i] = m * mesh->skeleton_info->skeleton_node_info[i].rest_object_space_to_object_space;
    }

    player->eval_count += 1;
}

Pose_Channel *get_primary_animation_channel(Animation_Player *player)
{
    if (!player) return NULL;

    for (i64 it_index = player->channels.count - 1; it_index >= 0; --it_index)
    {
        auto channel = player->channels[it_index];
        if (channel->type != Pose_Channel_Type::ANIMATION) continue;

        return channel;
    }

    return NULL;
}
