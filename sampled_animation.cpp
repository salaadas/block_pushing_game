//
// - How do we get the frames per second of the animation?
// - Why is the walking animation glitching back to the first one
// when being played?
//

#include "sampled_animation.h"

#include <cgltf.h>
// #include <ufbx.h>
#include "file_utils.h"

#include <glm/gtc/type_ptr.hpp> // for glm::make_mat4

//
// Right now, we are loading keyframed animations from gltf only.
//
// This is what I don't like about the gltf format: I don't know
// how to export models' animations with the right scale.
// 
// But we are using the samples from Khronos for testing right now
// so that is a issue with blender that we can deal with later.
//

// @Copypasta from mesh_catalog.cpp
template <typename T>
inline
void load_accessor_into_memory_buffer(cgltf_accessor *accessor, T **memory_buffer_address, i64 *desired_count = NULL, cgltf_type *desired_type = NULL)
{
    assert(accessor->buffer_view && accessor->buffer_view->buffer);

    auto gltf_buffer   = accessor->buffer_view->buffer;
    auto buffer_offset = accessor->offset + accessor->buffer_view->offset;

    *memory_buffer_address = reinterpret_cast<T*>(reinterpret_cast<u8*>(gltf_buffer->data) + buffer_offset);

    if (desired_count) *desired_count = accessor->count;    
    if (desired_type)  *desired_type  = accessor->type;
}

template <typename T>
bool get_pose_at_time(T *output, f32 time_stamp, cgltf_animation_sampler *time_sampler, SArr<T> reference)
{
    //
    // Incomplete: Only doing linear interpolation here. We are supposed to also
    // do step and cubic spline.
    //

    f32 start_time  = -1;
    f32 end_time    = -1;
    i64 frame_index = 0;

    auto input = time_sampler->input;
    for (i64 i = 0; i < (input->count - 1); ++i)
    {
        auto success = cgltf_accessor_read_float(input, i, &start_time, 1);
        if (!success) return false;

        success = cgltf_accessor_read_float(input, i + 1, &end_time, 1);
        if (!success) return false;

        if ((start_time <= time_stamp) && (time_stamp <= end_time))
        {
            frame_index = i;
            break;
        }
    }
    
    assert(start_time != -1);

    auto duration = end_time - start_time;
    constexpr f32 EPSILON = 0.0001f;
    if (fabsf(duration) <= EPSILON) return true; // Constant animation so we bail.

    auto fraction = (time_stamp - start_time) / duration;
    Clamp(&fraction, 0.0f, 1.0f);

    // If it is the last frame, don't interpolate, but return the thing itself.
    if (frame_index == (reference.count - 1))
    {
        *output = reference[reference.count - 1];
        return true;
    }

    auto transform0 = reference[frame_index];
    auto transform1 = reference[frame_index + 1];

    if constexpr (std::is_same<T, Vector3>::value)
    {
        *output = lerp(transform0, transform1, fraction);
    }
    else if constexpr (std::is_same<T, Vector4>::value)
    {
        // @Speed:
        // @Incomplete: We should also neighborhood the quaternions here so that
        // they are in the known-correct neighborhood. This is so that when 
        // we play the animation in the animation player, we don't have to do it.
        auto q0 = Quaternion(transform0.w, transform0.x, transform0.y, transform0.z);
        auto q1 = Quaternion(transform1.w, transform1.x, transform1.y, transform1.z);
        q0      = nlerp(q0, q1, fraction);

        *output = Vector4(q0.x, q0.y, q0.z, q0.w);
    }
    else
    {
        assert(0);
    }

    return true;
}

bool load_sampled_animation(Sampled_Animation *anim, String full_path)
{
    auto c_path    = (char*)(temp_c_string(full_path));
    auto extension = find_character_from_right(full_path, '.');
    if (!extension) return false;
    advance(&extension, 1); // Skip the '.'
    assert((extension == String("keyframe_animation"))); // Only this type of file is allowed.

    cgltf_options loader_options = {};
    cgltf_data *parsed_gltf_data = NULL;
    auto success = cgltf_parse_file(&loader_options, c_path, &parsed_gltf_data);
    if (success != cgltf_result_success)
    {
        logprint("load_sampled_animation", "Not able to parsed animation from file '%s'!\n", c_path);
        return false;
    }

    defer { cgltf_free(parsed_gltf_data); };
    assert((parsed_gltf_data->file_type == cgltf_file_type_gltf) || (parsed_gltf_data->file_type == cgltf_file_type_glb));

    // Loading the buffer of the GLTF file, I think this will load
    // the .bin file that is exported along with the model.
    success = cgltf_load_buffers(&loader_options, parsed_gltf_data, c_path);
    if (success != cgltf_result_success)
    {
        logprint("load_sampled_animation", "Not able to parsed the .bin file from animation '%s'!\n", c_path);
        return false;
    }

    // Number of frames per second (frame_rate), arbitrary value.
    {
        constexpr i32 FRAMES_PER_SECOND = 25;
        anim->frame_rate = FRAMES_PER_SECOND;
    }

    // We are loading only the first animation of the model,
    // solely because we know we wouldn't put more than one
    // inside the exported file.
    if (parsed_gltf_data->animations_count > 1)
    {
        logprint("load_sampled_animation", "Warning: file '%s' contains more than one animation (there are %ld)! Using the first one...\n", c_path, parsed_gltf_data->animations_count);
    }

    auto animation_data = &parsed_gltf_data->animations[0];

    // Getting the duration of the animation.
    // We loop over the all the animation samplers,
    // and then find the max time.
    f32 duration = -1;
    for (i64 it_index = 0; it_index < animation_data->samplers_count; ++it_index)
    {
        auto sampler = &animation_data->samplers[it_index];

        //
        // Read the sampler's keyframe input, which are the timestamps.
        //
        i64 time_stamps_count  = 0;
        f32 *time_stamp_buffer = NULL;
        load_accessor_into_memory_buffer(sampler->input, &time_stamp_buffer, &time_stamps_count);

        if (sampler->input->component_type == cgltf_component_type_r_32f) // If the components are floats
        {
            // Since the time stamps are increasing, we compare the last time stamp against
            // the duration and find the max.
            auto last_time_stamp = time_stamp_buffer[time_stamps_count - 1];
            duration = std::max(duration, last_time_stamp);
        }
    }
    anim->duration = duration;

    // The number of samples.
    i32 samples_count = anim->duration * anim->frame_rate;
    anim->num_samples = samples_count;

    // Get the nodes count. In order to attain the count, we need to export
    // the skinning info to get the skeleton nodes count. (This only applies to GLTF).
    auto skeletons_count = parsed_gltf_data->skins_count;
    assert(skeletons_count);
    if (skeletons_count > 1)
    {
        logprint("load_sampled_animation", "The sampled animation '%s' should only contains one skeleton, but we found %ld upon loading! Using the first one...\n", c_path, skeletons_count);
    }
    
    auto skeleton_data = &parsed_gltf_data->skins[0]; // Using the first skeleton, always.
    auto nodes_count   = skeleton_data->joints_count; // Nodes here means the skeleton nodes.
    assert(nodes_count); // Because we are doing skeletal animation, there's better gotta be at least one node.

    //
    // Making the mapping lookup for getting the skeleton node index from the
    // canonical node index.
    // This is because not every node is a skeleton node.
    //
    RArr<i32> node_to_joint_indices;
    node_to_joint_indices.allocator = {global_context.temporary_storage, __temporary_allocator};

    // We allocate the amount equivalent to all the nodes existed in the model, because
    // the amount of canonical nodes >= skeleton nodes.
    array_resize(&node_to_joint_indices, parsed_gltf_data->nodes_count);
    memset(node_to_joint_indices.data, -1, sizeof(i32) * node_to_joint_indices.count);

    auto nodes_info = NewArray<Node_Info>(nodes_count); // Node_Info of the skeleton.
    bool found_root = false; // There can only be one root!!!

    Matrix4 g_matrix(1.0f);

    // Making the lookup index table as well as the nodes info.
    for (i64 it_index = 0; it_index < nodes_count; ++it_index)
    {
        auto skeleton_node        = skeleton_data->joints[it_index];
        auto canonical_node_index = cgltf_node_index(parsed_gltf_data, skeleton_node);

        // Because we memset the whole node_to_joint lookup array to -1 at the start,
        // if the node has a parent but the parent is not a skeleton node (for example,
        // maybe the parent of the initial node is the 'Armature' node!), in this case,
        // we set the thing to -1, and also set the found_root flag.
        // If any subsequent node has a -1 index for its parent, we know that it is wrong.
        auto parent       = skeleton_node->parent;
        auto parent_index = node_to_joint_indices[cgltf_node_index(parsed_gltf_data, parent)];

        // @Speed: Only need this during development??? Can be #if out?
        if (parent_index == -1)
        {
            assert(!found_root);
            found_root = true;

            auto parent_of_the_root = skeleton_node->parent;
            if (parent_of_the_root)
            {
                cgltf_float parent_m[16];
                cgltf_node_transform_world(parent_of_the_root, parent_m);
                g_matrix = glm::make_mat4(parent_m);
            }
        }

        auto info = &nodes_info[it_index]; // Node info for the skeleton node.
        info->name = copy_string(String(skeleton_node->name));
        info->parent_index = parent_index;

        node_to_joint_indices[canonical_node_index] = it_index; // Mapping from the canonical node index to the skeleton node.
    }

    assert(found_root);

    anim->nodes_info = nodes_info;
    anim->g_matrix   = g_matrix;

    struct Node_Xform_Reference
    {
        // These samplers are for getting the time stamps.
        cgltf_animation_sampler *t_sampler;
        cgltf_animation_sampler *s_sampler;
        cgltf_animation_sampler *o_sampler;

        // Do not free these arrays since cgltf is in control of their memory.
        SArr<Vector3> translations;
        SArr<Vector3> scales;
        SArr<Vector4> orientations;
    };

    //
    // Given an index to a skeleton node, this will return the transformations
    // for that specific node throughout the animation duration.
    // It is used when we get the Xform_States for each Keyframe, because there,
    // we can map from each time stamp to the value desired.
    //
    RArr<Node_Xform_Reference> xform_references;
    array_resize(&xform_references, nodes_count); // @Cleanup: Consider using a pool, so that we can drop the whole thing on the floor when done loading all sampled animations.
    defer { array_free(&xform_references); };

    memset(xform_references.data, 0, sizeof(Node_Xform_Reference) * xform_references.count); // Clear everything to zero so that the counts of references are zero.

    //
    // This pass we get all the references to the transforms. These values will be
    // used in the pass right after, which processes every node for every keyframe.
    //
    {
        auto channels_count = animation_data->channels_count;
        for (i64 it_index = 0; it_index < channels_count; ++it_index)
        {
            auto channel = &animation_data->channels[it_index];
            auto sampler = channel->sampler;

            assert(channel->target_node);
            
            auto canonical_node_index = cgltf_node_index(parsed_gltf_data, channel->target_node);
            auto skeleton_node_index  = node_to_joint_indices[canonical_node_index];

            i64 xforms_count   = 0;
            u32 *xforms_buffer = NULL;

            cgltf_type accessor_type;
            load_accessor_into_memory_buffer(sampler->output, &xforms_buffer, &xforms_count, &accessor_type);

            auto xform_type = channel->target_path;
            auto xr = &xform_references[skeleton_node_index];

            switch (accessor_type)
            {
                case cgltf_type_vec3: {
                    auto output_buffer = reinterpret_cast<Vector3*>(xforms_buffer);

                    SArr<Vector3> *reference;
                    if (xform_type == cgltf_animation_path_type_translation)
                    {
                        reference = &xr->translations;
                        xr->t_sampler = sampler;
                    }
                    else if (xform_type == cgltf_animation_path_type_scale)
                    {
                        reference = &xr->scales;
                        xr->s_sampler = sampler;
                    }
                    else
                    {
                        assert(0); // Only translations and scales are allowed in this town.
                    }

                    assert(reference->count == 0); // A node should not have duplicate channels.
                    reference->count = xforms_count;
                    reference->data  = output_buffer; // Stealing the pointer from cgltf.
                } break;
                case cgltf_type_vec4: {
                    auto output_buffer = reinterpret_cast<Vector4*>(xforms_buffer);

                    assert(xform_type == cgltf_animation_path_type_rotation);
                    auto reference = &xform_references[skeleton_node_index].orientations;

                    assert(reference->count == 0); // A node should not have duplicate channels.
                    reference->count = xforms_count;
                    reference->data  = output_buffer; // Stealing the pointer from cgltf.

                    xr->o_sampler = sampler;
                } break;
                default: {
                    logprint("load_sampled_animation", "Receieved unexpected type for the transform type of output accessor (%d)!\n", accessor_type);
                    assert(0);
                }
            }
        }
    }

    //
    // For each and every keyframe, we loop through all the nodes.
    // Based on the time stamp calculated from the frame index, we
    // choose the right interpolated transformations of each nodes.
    //
    // The results for this will be stored in the xform states of
    // the 'keyframes' field in the sampled animation.
    //
    auto total_frames = anim->num_samples;
    auto keyframes    = NewArray<Keyframe>(total_frames);

    for (i64 frame_index = 0; frame_index < total_frames; ++frame_index)
    {
        auto time_stamp = frame_index / static_cast<f32>(anim->frame_rate - 1); // Subtracting by 1 here because we want the frame_index to range from [0..1]

        auto frame = &keyframes[frame_index];
        // For each frame, we store the transformations of all the skeleton nodes.
        frame->xform_states = NewArray<Xform_State>(nodes_count, false);

        for (i64 node_index = 0; node_index < nodes_count; ++node_index)
        {
            // // @Investigate: Are these transformations for the node in the bind pose?
            // auto node = skeleton_data->joints[node_index];
            // auto ot = node->translation;
            // auto oo = node->rotation;
            // auto os = node->scale;

            auto reference = &xform_references[node_index];

            assert(reference->translations.count);
            assert(reference->scales.count);
            assert(reference->orientations.count);

            bool success = false;

            Vector3 t;
            success = get_pose_at_time(&t, time_stamp, reference->t_sampler, reference->translations);
            assert(success);

            Vector3 s;
            success = get_pose_at_time(&s, time_stamp, reference->s_sampler, reference->scales);
            assert(success);

            Vector4 vo;
            success = get_pose_at_time(&vo, time_stamp, reference->o_sampler, reference->orientations);
            assert(success);

            Quaternion ori;
            ori.x = vo.x;
            ori.y = vo.y;
            ori.z = vo.z;
            ori.w = vo.w;

            auto state = &frame->xform_states[node_index];
            state->translation = t;
            state->scale       = s;
            state->orientation = ori;
        }
    }

    anim->keyframes = keyframes;

    return true;
}



















#if 0

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

    //         auto node_info  = &nodes_info[node_index];
    //         node_info->name = node_name;

    //         if (parent == NULL)
    //         {
    //             node_info->parent_index = -1;

    //             // Since the gltf matrix is column-major, we can do a memcpy
    //             // to the Matrix4 we are using!
    //             cgltf_float gltf_matrix[16];
    //             cgltf_node_transform_local(node, gltf_matrix);

    //             Matrix4 model_matrix;
    //             memcpy(&model_matrix[0][0], gltf_matrix, sizeof(cgltf_float) * 16);
    //             anim->model_matrix = model_matrix;
    //         }
    //         else
    //         {
    //             node_info->parent_index = cgltf_node_index(parsed_gltf_data, parent);
    //         }
    //     }
    // }

#endif 
