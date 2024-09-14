#include "sampled_animation.h"

#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

// #include <cgltf.h> :DeprecateMe
#include <ufbx.h>
#include "file_utils.h"

//
// Right now, we are loading keyframed animations from gltf only.
//
// This is what I don't like about the gltf format:
// - The animation does not have bones that are
//   in the bind pose position. Rather it uses
//   bones from the first frame of the animation.
// - Because of the reason above, I would have to
//   reload all the bones again and again every
//   time I load an animation.
// I will try and do it this way for a while and let's
// hope that I will switch to another smooth-brain format
// later.
//
// Because of all the problems I had with exporting gltf, I'm using fbx for now....
//

bool load_sampled_animation(Sampled_Animation *anim, String full_path)
{
    auto c_path    = (char*)(temp_c_string(full_path));
    auto extension = find_character_from_right(full_path, '.');
    advance(&extension, 1); // Skip the '.'
    // assert((extension == String("keyframe_animation"))); // Only this type of file is allowed. @Incomplete:

    ufbx_error fbx_error;
    ufbx_load_opts load_options = {};
    load_options.target_axes        = ufbx_axes_right_handed_z_up;
    load_options.target_unit_meters = 1.0f;
    load_options.space_conversion   = UFBX_SPACE_CONVERSION_MODIFY_GEOMETRY;

    auto scene_data = ufbx_load_file(c_path, &load_options, &fbx_error);
    if (!scene_data)
    {
        logprint("sampled_animation", "Failed to load the animation file '%s' due to error: %s!\n", c_path, fbx_error.description.data);
        return false;
    }

    defer { ufbx_free_scene(scene_data); };

    // {
    //     auto nodes = scene_data->nodes;
    //     auto nodes_count = 0;
    //     for (i32 i = 0; i < scene_data->nodes.count; ++i)
    //     {
    //         auto node = nodes[i];

    //         // @Incomplete: Do something for the root.
    //         if (node->is_root)
    //         {
    //             // printf("got a root at index %d!\n", i);
    //         }
    //         // Skipping if it is a mesh node.
    //         if (node->mesh) continue;

    //         // printf("Node %d's name is '%s'\n", nodes_count, node->name.data);

    //         nodes_count += 1;
    //     }
    // }

    assert(scene_data->anim_stacks.count >= 1);
    // Using the first animation, always.
    {
        auto orig_anim  = scene_data->anim_stacks[0]->anim;
        auto baked_anim = ufbx_bake_anim(scene_data, orig_anim, NULL, &fbx_error);
        if (!baked_anim)
        {
            logprint("sampled_animation", "Failed to bake animation from file '%s' due to error: %s!\n", c_path, fbx_error.description.data);
            return false;
        }

        defer { ufbx_free_baked_anim(baked_anim); };

        printf("playback time begin %f\tplayback time end %f\t playback duration %f\n", baked_anim->playback_time_begin, baked_anim->playback_time_end, baked_anim->playback_duration);

        printf("key time min %f\t key time max %f\n", baked_anim->key_time_min, baked_anim->key_time_max);

        for (i64 i = 0; i < baked_anim->nodes.count; ++i)
        {
            auto baked_node = &baked_anim->nodes[i];
            auto orig_node  = scene_data->nodes[baked_node->typed_id];
            auto parent     = orig_node->parent;

            if (parent)
            {
                printf("Node id for '%s' is %d, parent '%s' has id %d\n", orig_node->name.data, orig_node->typed_id, parent->name.data, parent->typed_id);
            }
            else
            {
                printf("Root is '%s' with id %d\n", orig_node->name.data, orig_node->typed_id);
            }

            // if (baked_node->constant_translation)
            // {
            //     printf("This is contant translation!\n");
            // }

            printf("Number of translation keys: %ld\n", baked_node->translation_keys.count);
            for (auto &t : baked_node->translation_keys)
            {
                auto time_stamp  = t.time;
                auto translation = Vector3(t.value.x, t.value.y, t.value.z);

                printf("At %f of '%s': trans is (%f %f %f)\n", time_stamp, orig_node->name.data, translation.x, translation.y, translation.z);
            }

            printf("Number of rotation keys: %ld\n", baked_node->rotation_keys.count);
            for (auto &o : baked_node->rotation_keys)
            {
                auto time_stamp  = o.time;
                Quaternion orientation;
                orientation.x = o.value.x;
                orientation.y = o.value.y;
                orientation.z = o.value.z;
                orientation.w = o.value.w;

                // printf("At %f of '%s': ori is (%f %f %f %f)\n", time_stamp, orig_node->name.data, orientation.x, orientation.y, orientation.z, orientation.w);
            }

            printf("Number of scale keys: %ld\n", baked_node->scale_keys.count);
            for (auto &s : baked_node->scale_keys)
            {
                auto time_stamp = s.time;
                auto scale      = Vector3(s.value.x, s.value.y, s.value.z);

                // printf("At %f of '%s': scale is (%f %f %f)\n", time_stamp, orig_node->name.data, scale.x, scale.y, scale.z);
            }

            newline();
        }
    }

    return true;
}

#if 0

//
// @Speed!!! This is using a slow ass version of the animation loading....
// @Speed!!! This is using a slow ass version of the animation loading....
// @Speed!!! This is using a slow ass version of the animation loading....
// @Speed!!! This is using a slow ass version of the animation loading....
//
bool load_sampled_animation(Sampled_Animation *anim, String full_path)
{
    auto c_path    = (char*)(temp_c_string(full_path));
    auto extension = find_character_from_right(full_path, '.');
    advance(&extension, 1); // Skip the '.'
    assert((extension == String("keyframe_animation"))); // Only this type of file is allowed.

    cgltf_options loader_options = {};
    cgltf_data *parsed_gltf_data = NULL;
    auto parsed_result = cgltf_parse_file(&loader_options, c_path, &parsed_gltf_data);
    if (parsed_result != cgltf_result_success)
    {
        logprint("load_sampled_animation", "Not able to parsed animation from file '%s'!\n", c_path);
        return false;
    }

    defer { cgltf_free(parsed_gltf_data); };
    assert((parsed_gltf_data->file_type == cgltf_file_type_gltf) || (parsed_gltf_data->file_type == cgltf_file_type_glb));

    // Loading the buffer of the GLTF file, I think this will load the .bin file
    //  that is exported along with the model.
    parsed_result = cgltf_load_buffers(&loader_options, parsed_gltf_data, c_path);
    if (parsed_result != cgltf_result_success)
    {
        logprint("load_sampled_animation", "Not able to parsed the .bin file from animation '%s'!\n", c_path);
        return false;
    }

    // Number of frames per second (frame_rate)
    // For gltf format, this is 1000 frames per second.
    {
        constexpr i32 FRAMES_PER_SECOND = 25; // @Note: This is only specific for gltf format.
        anim->frame_rate = FRAMES_PER_SECOND;
    }

    // We are loading only the first animation of the model,
    // solely because we know we wouldn't put more than one
    // inside the exported file.
    auto animation_data = &parsed_gltf_data->animations[0];

    // The duration of the animation, in seconds.
    {
        f64 duration = -1;

        // We loop through all the samplers and read the last time stamp from there.
        // Then we do a max() on all of them to find the longest duration.
        auto samplers_count = animation_data->samplers_count;
        for (i64 sampler_index = 0; sampler_index < samplers_count; ++sampler_index)
        {
            auto sampler = &animation_data->samplers[sampler_index];

            f32 ending_time_stamp = 0;
            auto success = cgltf_accessor_read_float(sampler->input, sampler->input->count - 1, &ending_time_stamp, 1); // Reading the last time_stamp of the sampler.

            duration = std::max(duration, static_cast<f64>(ending_time_stamp));
        }

        assert((duration != -1)); // The given file should have reasonable duration by default.
        anim->duration = duration;
    }

    // The number of samples.
    {
        i32 number_of_samples = anim->duration * anim->frame_rate;
        anim->num_samples = number_of_samples;
    }

    // Node count and nodes info.
    auto nodes_count = parsed_gltf_data->nodes_count;
    {
        // printf("The nodes count is %ld\n", nodes_count);

        auto nodes_info  = NewArray<Sampled_Animation::Node_Info>(nodes_count); // @Leak: If we hotload, we leak.
        anim->nodes_info = nodes_info;

        for (i64 node_index = nodes_count - 1; node_index >= 0; --node_index)
        {
            auto node      = &parsed_gltf_data->nodes[node_index];
            auto node_name = copy_string(String(node->name));
            auto parent    = node->parent;

            auto node_info  = &nodes_info[node_index];
            node_info->name = node_name;

            if (parent == NULL)
            {
                node_info->parent_index = -1;

                // Since the gltf matrix is column-major, we can do a memcpy
                // to the Matrix4 we are using!
                cgltf_float gltf_matrix[16];
                cgltf_node_transform_local(node, gltf_matrix);

                Matrix4 model_matrix;
                memcpy(&model_matrix[0][0], gltf_matrix, sizeof(cgltf_float) * 16);
                anim->model_matrix = model_matrix;
            }
            else
            {
                node_info->parent_index = cgltf_node_index(parsed_gltf_data, parent);
            }
        }
    }

    {
        // For each of the frame:
        //   We get the translation, orientation, and scale of each node.
        //   (Maybe this is with respect to the bind pose).

        auto skins_count = parsed_gltf_data->skins_count;
        assert(skins_count == 1);
        auto skin_data    = &parsed_gltf_data->skins[0];
        auto joints_count = skin_data->joints_count;

        //
        // Zeroth pass: Make a indexing array that points from the index of
        // the node to the index of the joint.
        //
        // @Cleanup: We could consider moving this into the nodes_info of anim.
        RArr<i32> node_to_joint_indices;
        node_to_joint_indices.allocator = {global_context.temporary_storage, __temporary_allocator};
        array_reserve(&node_to_joint_indices, nodes_count);
        node_to_joint_indices.count = joints_count;

        for (i32 i = 0; i < joints_count; ++i)
        {
            auto joint      = skin_data->joints[i];
            auto node_index = cgltf_node_index(parsed_gltf_data, joint);
            
            node_to_joint_indices[node_index] = i; // This is the mapping from node index to joint index.
        }

        //
        // First pass: Go through all the animation channels, then
        // map it so that we can access the animation of each joint
        // via its index.
        //
        struct Channel_Info_For_Joint // This is very bad...
        {
            cgltf_interpolation_type interpolation_type;
            cgltf_animation_channel *translation_channel = NULL;
            cgltf_animation_channel *orientation_channel = NULL;
            cgltf_animation_channel *scale_channel       = NULL;
        };
        auto joints_channels = NewArray<Channel_Info_For_Joint>(joints_count); // nocheckin: remove me.

        for (i32 channel_index = 0; channel_index < animation_data->channels_count; ++channel_index)
        {
            auto channel     = &animation_data->channels[channel_index];
            auto target_node = channel->target_node;
            auto node_index  = cgltf_node_index(parsed_gltf_data, target_node);
            auto joint_index = node_to_joint_indices[node_index];

            auto sampler = channel->sampler;
            auto interp_type = sampler->interpolation;
            assert(interp_type != cgltf_interpolation_type_max_enum);

            auto joint_channel = &joints_channels[joint_index];
            joint_channel->interpolation_type = interp_type;

            switch (channel->target_path)
            {
                case cgltf_animation_path_type_translation: joint_channel->translation_channel = channel; break;
                case cgltf_animation_path_type_rotation:    joint_channel->orientation_channel = channel; break;
                case cgltf_animation_path_type_scale:       joint_channel->scale_channel = channel; break;
                default: {
                    logprint("sampled_animation", "Unexpected interpolation in animation channel for node '%s' of file '%s'!\n", target_node->name, c_path);
                    assert(0);
                }
            }
        }

        //
        // Second pass: For each keyframe, we go over all the joints to get their
        // transformations.
        //

        // I think interp_type should already be inside channel?
        // I think interp_type should already be inside channel?
        // I think interp_type should already be inside channel?
        auto get_pose_at_time = [](cgltf_interpolation_type interp_type, cgltf_animation_channel *channel, f32 time_stamp, void *resulting_transform) -> bool {
            assert(interp_type < cgltf_interpolation_type_max_enum);

            f32 time_start = 0;
            f32 time_end   = 0;
            i32 keyframe_index = 0;

            auto sampler = channel->sampler;
            auto input   = sampler->input;

            for (i32 i = 0; i < input->count; ++i)
            {
                auto success = cgltf_accessor_read_float(input, i, &time_start, 1);
                if (!success) return false;

                success = cgltf_accessor_read_float(input, i + 1, &time_end, 1);
                if (!success) return false;

                if ((time_start <= time_stamp) && (time_end <= time_stamp))
                {
                    keyframe_index = i;
                    break;
                }
            }

            auto duration = time_end - time_start;

            constexpr f32 EPSILON = 0.0001f;
            if (fabsf(duration) <= EPSILON) return true; // Constant animation so we bail.

            auto delta = (time_stamp - time_start) / duration;
            delta = std::clamp(delta, 0.0f, 1.0f);

            auto output = sampler->output;
            if (output->type == cgltf_type_vec3)
            {
                switch (interp_type)
                {
                    case cgltf_interpolation_type_linear: {
                        f32 temp[3];
                        cgltf_accessor_read_float(output, keyframe_index, temp, 3);
                        auto v0 = Vector3(temp[0], temp[1], temp[2]);

                        cgltf_accessor_read_float(output, keyframe_index + 1, temp, 3);
                        auto v1 = reinterpret_cast<Vector3*>(resulting_transform);
                        v1->x = temp[0];
                        v1->y = temp[1];
                        v1->z = temp[2];

                        *v1 = lerp(v0, *v1, delta);
                    } break;
                    case cgltf_interpolation_type_step: {
                        f32 temp[3];
                        cgltf_accessor_read_float(output, keyframe_index, temp, 3);

                        auto v = reinterpret_cast<Vector3*>(resulting_transform);
                        v->x = temp[0];
                        v->y = temp[1];
                        v->z = temp[2];
                    } break;
                    case cgltf_interpolation_type_cubic_spline: {
                        assert(0); // @Incomplete:
                    } break;
                }
            }
            else if (output->type == cgltf_type_vec4)
            {
                switch (interp_type)
                {
                    case cgltf_interpolation_type_linear: {
                        f32 temp[4];
                        cgltf_accessor_read_float(output, keyframe_index, temp, 4);
                        auto q0 = Quaternion(temp[0], temp[1], temp[2], temp[3]);

                        cgltf_accessor_read_float(output, keyframe_index + 1, temp, 4);
                        auto q1 = reinterpret_cast<Quaternion*>(resulting_transform);
                        q1->x = temp[0];
                        q1->y = temp[1];
                        q1->z = temp[2];
                        q1->w = temp[3];

                        *q1 = slerp(q0, *q1, delta);
                    } break;
                    case cgltf_interpolation_type_step: {
                        f32 temp[4];
                        cgltf_accessor_read_float(output, keyframe_index, temp, 4);

                        auto q = reinterpret_cast<Quaternion*>(resulting_transform);
                        q->x = temp[0];
                        q->y = temp[1];
                        q->z = temp[2];
                        q->w = temp[3];
                    } break;
                    case cgltf_interpolation_type_cubic_spline: {
                        assert(0); // @Incomplete:
                    } break;
                }
            }
            else
            {
                logprint("get_pose_at_time", "Unexpected type passed into get pose %d!!!!", output->type);
                return false;
            }

            return true;
        };

        auto total_frames = anim->num_samples;
        printf("Total frames is %d\n", total_frames);

        anim->keyframes = NewArray<Sampled_Animation::Keyframe>(total_frames);
        for (i32 frame_index = 0; frame_index < total_frames; ++frame_index)
        {
            // Adding one here because when we get the starting time stamp
            // and end timestamp of this keyframe, the starting time stamp
            // will need to be less than or equals to this time stamp.
            // Because of this, we will not lose out on the last frame of
            // the animation.
            auto time_stamp = (frame_index + 1) / static_cast<f32>(anim->frame_rate);

            auto keyframe = &anim->keyframes[frame_index];
            keyframe->xform_states = NewArray<Xform_State>(joints_count, false); // Ughh... We should collapse this with the 'keyframes' array.

            for (i32 joint_index = 0; joint_index < joints_count; ++joint_index)
            {
                auto joint       = skin_data->joints[joint_index];
                auto xform_state = &keyframe->xform_states[joint_index];

                auto t = joint->translation;
                Vector3 translation = Vector3(t[0], t[1], t[2]);

                auto o = joint->rotation;
                Quaternion orientation = Quaternion(o[0], o[1], o[2], o[3]);

                auto s = joint->scale;
                Vector3 scale = Vector3(s[0], s[1], s[2]);

                auto joint_channel = &joints_channels[joint_index];
                auto interp_type = joint_channel->interpolation_type;

                if (joint_channel->translation_channel)
                {
                    auto success = get_pose_at_time(interp_type, joint_channel->translation_channel, time_stamp, &translation);
                    if (!success) logprint("sampled_animation", "Failed to get translation pose for joint '%s' at time stamp %f in animation '%s'.", joint->name, time_stamp, c_path);
                }

                if (joint_channel->orientation_channel)
                {
                    auto success = get_pose_at_time(interp_type, joint_channel->orientation_channel, time_stamp, &orientation);
                    if (!success) logprint("sampled_animation", "Failed to get orientation pose for joint '%s' at time stamp %f in animation '%s'.", joint->name, time_stamp, c_path);
                }

                if (joint_channel->scale_channel)
                {
                    auto success = get_pose_at_time(interp_type, joint_channel->scale_channel, time_stamp, &scale);
                    if (!success) logprint("sampled_animation", "Failed to get scaled pose for joint '%s' at time stamp %f in animation '%s'.", joint->name, time_stamp, c_path);
                }

                xform_state->translation = translation;
                xform_state->orientation = orientation;
                xform_state->scale       = scale;

                printf("Translation for joint %d is (%f %f %f)\n", joint_index, translation.x, translation.y, translation.z);
            }
        }
    }

    return false;
}

inline
Matrix4 from_assimp_matrix4(const aiMatrix4x4 &from)
{
    // @Speed:
    Matrix4 to;
    // The a,b,c,d in assimp are the rows; The 1,2,3,4 are the columns.
    to[0][0] = from.a1; to[1][0] = from.a2; to[2][0] = from.a3; to[3][0] = from.a4;
    to[0][1] = from.b1; to[1][1] = from.b2; to[2][1] = from.b3; to[3][1] = from.b4;
    to[0][2] = from.c1; to[1][2] = from.c2; to[2][2] = from.c3; to[3][2] = from.c4;
    to[0][3] = from.d1; to[1][3] = from.d2; to[2][3] = from.d3; to[3][3] = from.d4;
    return to;
}

void walk_through_all_the_nodes_from(aiNode *assimp_node, i32 *node_count)
{
    assert(assimp_node);
    assert(node_count);
    
    auto node_name   = String(assimp_node->mName.data);
    auto parent_node = assimp_node->mParent; // Since we are not passing the root node in here. We should not get a NULL parent.
    assert((parent_node != NULL));

    *node_count += 1;

    auto children_count = assimp_node->mNumChildren;
    for (i32 i = 0; i < children_count; ++i)
    {
        auto child_node = assimp_node->mChildren[i];
        walk_through_all_the_nodes_from(child_node, node_count);
    }
}

bool load_sampled_animation(Sampled_Animation *anim, String full_path)
{
    auto c_path    = (char*)(temp_c_string(full_path));
    auto extension = find_character_from_right(full_path, '.');
    advance(&extension, 1); // Skip the '.'
    assert((extension == String("keyframe_animation"))); // Only this type of file is allowed.

    aiScene *assimp_model = const_cast<aiScene*>(aiImportFile(c_path, aiProcessPreset_TargetRealtime_Quality));
    defer { aiReleaseImport(assimp_model); };

    if (!assimp_model)
    {
        logprint("sampled_animation", "Failed to load the animation at path '%s'.\n", c_path);
        return false;
    }
    assert((assimp_model->mRootNode)); // If you get to here, you better have a model with a root node.

    // @Note: It is not always the case that the total animation channels count is the same as
    // the count of all the nodes of the mesh. Because of this, we need to walk through all the
    // nodes before processing the channels themselves.
    {
        i32 node_count = 0;

        auto children_count_of_the_root_node = assimp_model->mRootNode->mNumChildren;
        for (i32 i = 0; i < children_count_of_the_root_node; ++i)
        {
            auto child_of_root = assimp_model->mRootNode->mChildren[i];
            walk_through_all_the_nodes_from(child_of_root, &node_count);
        }

        anim->node_info = NewArray<Sampled_Animation::Node_Info>(node_count); // @Leak: If we hotload, we leak.
    }

    // Here, we are using the first animation of the loaded file because we load each
    // animation file separately. However, this approach to me is kind of bad and should
    // need a revision later, or we could do a loop on the number of different animations
    // inside a file and then load them. I'm not sure as we are still dependant on Assimp
    // and not our own animation/mesh loader.
    auto assimp_animation  = assimp_model->mAnimations[0];
    auto samples_per_node  = assimp_animation->mDuration; // This is actually duration count as number of frames.
    auto frames_per_second = assimp_animation->mTicksPerSecond;

    anim->frame_rate  = static_cast<i32>(frames_per_second);
    if (anim->frame_rate == 0)
    {
        logprint("sampled_animation", "the animation frame rate for '%s' was 0, setting it to 10...\n", c_path);
        anim->frame_rate = 10;
    }

    anim->num_samples = samples_per_node;
    anim->duration    = static_cast<f64>(anim->num_samples) / anim->frame_rate;

    aiMatrix4x4 assimp_global_transformation = assimp_model->mRootNode->mTransformation;
    // assimp_global_transformation.Inverse(); // Why???
    anim->global_matrix = from_assimp_matrix4(assimp_global_transformation);

    auto total_animation_channels = assimp_animation->mNumChannels;
    for (i32 channel_index = 0; channel_index < total_animation_channels; ++channel_index)
    {
        // From anim.h in external/assimp:
        // 
        // The 'channel' variable below describes the animation of a single node. 
        // This includes the bone/node which is affected by this animation channel.
        // The keyframes are given in three separate series of values:
        //   - One for translation.
        //   - One for rotation.
        //   - One for scaling.
        // Also, the transformation matrix computed from these values would replace
        // the node's original transformation matrix at a specific time.
        // This means that all the keys are absolute and not relative to the bone's
        // default pose.
        // The order that you should apply the transformation is:
        //   scale -> rotate -> translate.
        auto channel = assimp_animation->mChannels[channel_index];

        // This is the name of the node in which this animation channel is applying for.
        String affected_node = String(channel->mNodeName.data);

        i32 total_translations = channel->mNumPositionKeys;
        i32 total_rotations    = channel->mNumRotationKeys;
        i32 total_scalings     = channel->mNumScalingKeys;

        for (i32 translation_index = 0; translation_index < total_translations; ++translation_index)
        {
            auto translation_keyframe = channel->mPositionKeys[translation_index];
            auto assimp_translation   = translation_keyframe.mValue;
            auto translation_vector   = Vector3(assimp_translation.x, assimp_translation.y, assimp_translation.z);
            f64 keyframe_time = translation_keyframe.mTime;
        }
    }
    
    // @Incomplete:
    return false;
}

#endif
