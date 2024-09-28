#include "animation_hud.h"

#include "opengl.h"
#include "draw.h"
#include "font.h"
#include "main.h"
#include "sokoban.h"
#include "time_info.h"
#include "animation_player.h"
#include "animation_channel.h"

Dynamic_Font *animation_hud_font = NULL;
Dynamic_Font *small_font = NULL;

// @Refactor: We should make this takes an Entity* instead of a Guy*,
// which will make us put animation_state into the Entity struct.
void draw_animation_info(Guy *guy)
{
    auto e = guy->base;
    if (!e->animation_player) return;

    //
    // 3D rendering time:
    //
    set_matrix_for_entities(e->manager, Vector2(0, 0)); // @Incomplete: Not handling offset (which is not an issue right now).

    //
    // Drawing the mesh's triangles.
    //
    {
        glEnable(GL_DEPTH_TEST);

        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(1, 1);
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

        auto outline_color = Vector4(.8, .3, .9, 1);

        constexpr auto SELECTION_STROBE_RATE = 4.f;
        
        auto t = cosf(timez.current_time * SELECTION_STROBE_RATE);
        outline_color = lerp(outline_color, Vector4(1, 1, 1, 1), t);

        XXX_hack_drawing_selection = true;
        draw_guy_at(guy, false, false, &outline_color);
        XXX_hack_drawing_selection = false;

        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glDisable(GL_POLYGON_OFFSET_FILL);

        glDisable(GL_DEPTH_TEST);
    }

    set_shader(shader_argb_no_texture);

    // @Fixme: We would like to use this instead of the @Hack below.
    // set_object_to_world_matrix(e->visual_position, e->orientation, e->scale);

    auto ori = e->orientation; // @Hack: This is because right now the models are not exported in the correct-facing way. We should fix this in Blender, not in here.
    Quaternion mesh_correction;
    get_ori_from_rot(&mesh_correction, Vector3(0, 0, 1), MESH_RENDER_THETA_OFFSET * (TAU / 360.0));
    ori = ori * mesh_correction;

    set_object_to_world_matrix(e->visual_position, ori, e->scale);

    //
    // Drawing the skeleton.
    //
    auto aplayer = e->animation_player;
    immediate_begin();

    i32 it_index = 0;
    for (auto &state : aplayer->current_states)
    {
        auto mesh_parent_index = aplayer->mesh_parent_indices[it_index];
        if (mesh_parent_index < 0)
        {
            it_index += 1;
            continue;
        }

        Matrix4 m;

        get_matrix(state, &m);
        auto my_pos4 = m * Vector4(0, 0, 0, 1);
        auto my_pos  = Vector3(my_pos4);

        get_matrix(aplayer->current_states[mesh_parent_index], &m);
        auto parent_pos4 = m * Vector4(0, 0, 0, 1);
        auto parent_pos  = Vector3(parent_pos4);

        auto bone_dir = unit_vector(parent_pos - my_pos);

        constexpr auto EPSILON = 0.00001f;
        if (glm::length(bone_dir) < EPSILON)
        {
            it_index += 1;
            continue;
        }

        auto camera_forward = e->manager->camera.forward_vector;
        auto [vy, vz] = make_an_orthonormal_basis(bone_dir, camera_forward);

        auto b = 0.01f; // Thickness of the strut in the world space...
        auto strut = vy * b;
        auto p0 = my_pos - strut;
        auto p1 = my_pos + strut;
        auto p2 = parent_pos + strut;
        auto p3 = parent_pos - strut;

        immediate_quad(p0, p1, p2, p3, 0xff00ffff);

        it_index += 1;
    }
    immediate_flush();

    //
    // 2D rendering time:
    //

    RArr<String> lines;
    lines.allocator = {global_context.temporary_storage, __temporary_allocator};

    RArr<String> left_lines; // For Animation_Channel blend factors.
    left_lines.allocator = {global_context.temporary_storage, __temporary_allocator};

    RArr<String> right_lines; // For Animation_Channel current times.
    right_lines.allocator = {global_context.temporary_storage, __temporary_allocator};

    for (auto it : aplayer->channels)
    {
        if (it->animation)
        {
            array_add(&lines, it->animation->name);
            array_add(&left_lines, tprint(String("%.3f"), it->blend_factor_for_debug_output));
            array_add(&right_lines, tprint(String("%.3f"), it->current_time));
        }
    }

    i64 highest_width = 0;
    for (auto line : lines)
    {
        highest_width = std::max(highest_width, get_text_width(small_font, line));
    }

    i64 highest_left_width = 0;
    for (auto line : left_lines)
    {
        highest_left_width = std::max(highest_left_width, get_text_width(small_font, line));
    }

    i64 highest_right_width = 0;
    for (auto line : right_lines)
    {
        highest_right_width = std::max(highest_right_width, get_text_width(small_font, line));
    }

    //
    // Animation blending/state information.
    //
    auto no_rotation = Quaternion(1, 0, 0, 0);
    auto name_offset = Vector3(-.6f, -.5f, 0);
    set_object_to_world_matrix(e->visual_position + name_offset, no_rotation, e->scale);

    auto projected = object_to_proj_matrix * Vector4(0, 0, 0, 1);
    projected.x /= projected.w;
    projected.y /= projected.w;
    projected.z /= projected.w;
    projected.w /= projected.w;

    f32 text_x = (projected.x * .5f + .5f) * render_target_width;
    f32 text_y = (projected.y * .5f + .5f) * render_target_height;

    auto s = &guy->animation_state;
    auto node_name = s->node_name; // What state we are in.

    auto font = animation_hud_font;

    auto node_width = prepare_text(font, node_name);
    highest_width   = std::max(highest_width, node_width); // Include the width of the current node in highest_width too!

    auto x = static_cast<i32>(text_x);
    auto y = static_cast<i32>(text_y);

    //
    // Drawing the backing box!
    //
    auto margin = small_font->character_height / 2.0f;
    {
        auto box_top = y + font->character_height;

        auto x0 = x - highest_left_width - margin * 2.0f;
        auto y1 = box_top + margin;
        auto y0 = y1 - lines.count * small_font->default_line_spacing - margin * 2.0f;
        y0 = y0 - lines.count - font->character_height - small_font->character_height; // Accounts for the bumped line and the current node name.
        auto x1 = x + highest_width + highest_right_width + margin;

        rendering_2d_right_handed();
        set_shader(shader_argb_no_texture);
        immediate_quad(Vector3(x0, y0, 0), Vector3(x1, y0, 0), Vector3(x1, y1, 0), Vector3(x0, y1, 0), 0xdd010101);
    }

    // Now, we draw the current node name!
    draw_prepared_text_with_backing(font, x, y, Vector4(.2f, .9f, .88f, 1));

    y -= small_font->character_height; // Bumps all the stuff below down a bit.
    y -= small_font->character_height;

    //
    // Drawing the Animation_Player state.
    //
    auto starting_y = y;
    for (auto text : lines)
    {
        auto width = prepare_text(small_font, text);
        draw_prepared_text_with_backing(small_font, x, y, Vector4(.7f, .9f, .88f, 1));

        y -= small_font->default_line_spacing;
    }

    //
    // Drawing the blend factors.
    //
    y = starting_y;
    auto starting_x = x;
    x -= highest_left_width + margin;
    for (auto text : left_lines)
    {
        auto width = prepare_text(small_font, text);
        draw_prepared_text_with_backing(small_font, x, y, Vector4(1.f, 1.f, .88f, 1));

        y -= small_font->default_line_spacing;
    }

    //
    // Drawing the current time for each channels.
    //
    y = starting_y;
    x = starting_x + highest_width + margin;
    for (auto text : right_lines)
    {
        auto width = prepare_text(small_font, text);
        draw_prepared_text_with_backing(small_font, x, y, Vector4(.85f, .7f, .9f, 1));

        y -= small_font->default_line_spacing;
    }

    immediate_flush();

    object_to_world_matrix = Matrix4(1.0f); // Set it back to be nice?
}

void draw_animation_hud()
{
    rendering_2d_right_handed_unit_scale();

    if (was_window_resized_this_frame || !animation_hud_font)
    {
        animation_hud_font = get_font_at_size(FONT_FOLDER, String("DejaVuSans-Bold.ttf"), BIG_FONT_SIZE * .3f);
        small_font         = get_font_at_size(FONT_FOLDER, String("DejaVuSans-Bold.ttf"), BIG_FONT_SIZE * .2f);
    }

    auto manager = get_entity_manager();
    for (auto it : manager->_by_guys)
    {
        draw_animation_info(it);
    }
}
