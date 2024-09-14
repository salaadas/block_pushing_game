#include "draw.h"

#include "main.h"
#include "opengl.h"
#include "time_info.h"
#include "file_utils.h"
#include "camera.h"
#include "mesh_catalog.h"
#include "sokoban.h"
#include "editor.h"

Shader *shader_argb_no_texture;
Shader *shader_argb_and_texture;
Shader *shader_text;
Shader *shader_basic_3d;

#define cat_find(catalog, name) catalog_find(&catalog, String(name));

Triangle_Mesh *red_guy_mesh;

void init_shaders()
{
    shader_argb_no_texture  = cat_find(shader_catalog, "argb_no_texture"); assert(shader_argb_no_texture);
    shader_argb_and_texture = cat_find(shader_catalog, "argb_and_texture"); assert(shader_argb_and_texture);
    shader_text             = cat_find(shader_catalog, "text"); assert(shader_text);
    shader_basic_3d         = cat_find(shader_catalog, "basic_3d"); assert(shader_basic_3d);

    shader_argb_no_texture->backface_cull  = false;
    shader_argb_and_texture->backface_cull = false;

    { // :DeprecateMe testing out the new model loader.
        red_guy_mesh = catalog_find(&mesh_catalog, String("ok"));
        assert(red_guy_mesh != NULL);
    }
}

#undef cat_find

void rendering_2d_right_handed()
{
    f32 w = render_target_width;
    f32 h = render_target_height;
    if (h < 1) h = 1;

    // This is a GL-style projection matrix mapping to [-1, 1] for x and y
    Matrix4 tm = Matrix4(1.0);
    tm[0][0] = 2 / w;
    tm[1][1] = 2 / h;
    tm[3][0] = -1;
    tm[3][1] = -1;

    view_to_proj_matrix    = tm;
    world_to_view_matrix   = Matrix4(1.0);
    object_to_world_matrix = Matrix4(1.0);

    refresh_transform();
}

void rendering_2d_right_handed_unit_scale()
{
    // @Note: cutnpaste from rendering_2d_right_handed
    f32 h = render_target_height / (f32)render_target_width;

    // This is a GL-style projection matrix mapping to [-1, 1] for x and y
    auto tm = Matrix4(1.0);
    tm[0][0] = 2;
    tm[1][1] = 2 / h;
    tm[3][0] = -1;
    tm[3][1] = -1;

    view_to_proj_matrix    = tm;
    world_to_view_matrix   = Matrix4(1.0);
    object_to_world_matrix = Matrix4(1.0);

    refresh_transform();
}

void print_vec3(Vector3 v)
{
    printf("{%f %f %f}\n", v.x, v.y, v.z);
}

void do_block(Vector3 p0, Vector3 p1, Vector3 p2, Vector3 p3,
              Vector3 p4, Vector3 p5, Vector3 p6, Vector3 p7,
              u32 color)
{
    immediate_quad(p3, p2, p1, p0, Vector3( 0,  0, -1), color);
    immediate_quad(p4, p5, p6, p7, Vector3( 0,  0,  1), color);
    immediate_quad(p0, p4, p7, p3, Vector3(-1,  0,  0), color);
    immediate_quad(p5, p1, p2, p6, Vector3( 1,  0,  0), color);
    immediate_quad(p0, p1, p5, p4, Vector3( 0, -1,  0), color);
    immediate_quad(p7, p6, p2, p3, Vector3( 0,  1,  0), color);
}

// @Hack @Hack @Hack: Replace this material color hack when we do the full material system for our shaders.
void set_material_color(Vector4 color)
{
    auto shader = current_shader;
    assert((shader == shader_basic_3d));

    auto loc = glGetUniformLocation(shader->program, "material_color");
    if (loc < 0)
    {
        logprint("set_material_color", "Could not find the material color for current shader '%s'.\n", temp_c_string(shader->name));
        DumpGLErrors("set_material_color");
        return;
    }

    glUniform4fv(loc, 1, (f32*)&color);
}

// Position here is the center of the back pane???
void draw_block_entity_at(Vector3 position, Texture_Map *map, f32 radius,
                          bool dimmed = false, bool dead = false, bool mirrored = false,
                          Vector4 *color_ptr = NULL, Vector2 *uv_offset = NULL)
{
    if (!map) return;

    auto p0 = position;
    auto p1 = position;
    auto p2 = position;
    auto p3 = position;
    auto b  = radius;

    p0.x -= b;
    p0.y -= b;

    p1.x += b;
    p1.y -= b;

    p2.x += b;
    p2.y += b;

    p3.x -= b;
    p3.y += b;

    if (mirrored)
    {
        p0.x += 2*b;
        p1.x -= 2*b;
        p2.x -= 2*b;
        p3.x += 2*b;
    }

    auto p4 = p0;
    auto p5 = p1;
    auto p6 = p2;
    auto p7 = p3;

    p4.z += 1;
    p5.z += 1;
    p6.z += 1;
    p7.z += 1;

    set_texture(String("diffuse_texture"), map);

    Vector4 fcolor;

    if (dimmed || dead)
    {
        auto k = 0.4f;
        if (color_ptr) fcolor = Vector4(color_ptr->x*k, color_ptr->y*k, color_ptr->z*k, color_ptr->w);
        else fcolor = Vector4(k, k, k, 1);
    }
    else
    {
        if (color_ptr) fcolor = Vector4(color_ptr->x, color_ptr->y, color_ptr->z, color_ptr->w);
        else fcolor = Vector4(1, 1, 1, 1);
    }

    set_material_color(fcolor);

    auto color = argb_color(fcolor);
    do_block(p0, p1, p2, p3, p4, p5, p6, p7, color);
}

void draw_gradient()
{
    rendering_2d_right_handed();

    f32 w = render_target_width;
    f32 h = render_target_height;

    constexpr i32 PADDING = 100;

    auto p0 = Vector2(0 + PADDING, 0 + PADDING);
    auto p1 = Vector2(w - PADDING, 0 + PADDING);
    auto p2 = Vector2(w - PADDING, h - PADDING);
    auto p3 = Vector2(0 + PADDING, h - PADDING);

    f32 r0 = 61.0f / 255.0f;
    f32 g0 = 29.0f / 255.0f;
    f32 b0 = 29.0f / 255.0f;

    f32 r1 = 35.0f / 255.0f;
    f32 g1 = 19.0f / 255.0f;
    f32 b1 = 19.0f / 255.0f;

    f32 k0 = 0.7f;
    f32 k1 = 0.3f;

    r0 *= k0;
    g0 *= k0;
    b0 *= k0;

    r1 *= k1;
    g1 *= k1;
    b1 *= k1;

    auto z = 0.0f;

    auto c0 = argb_color(Vector3(r0, g0, b0));
    auto c1 = argb_color(Vector3(r1, g1, b1));

    auto background_color = argb_color(Vector3(1, 1, .12));

    set_shader(shader_argb_no_texture);
    immediate_begin();
    immediate_quad(p0, p1, p2, p3, background_color);

    {
        auto my_r = (f32)((i32)(timez.ui_time * 14) & 255) / 255.0;
        auto my_g = (f32)((i32)(timez.ui_time * 10) & 255) / 255.0;

        immediate_triangle(p0, p1, p3, argb_color(Vector3(my_r, my_g, .5)));
    }

    immediate_flush();
}

void draw_text_with_backing(Dynamic_Font *font, i64 x, i64 y, String text, Vector4 color);

void draw_centered(Dynamic_Font *font, f32 y_unit_scale, String text, Vector4 color)
{
    auto width = prepare_text(font, text);

    i64 x = (i64)((render_target_width - width) / 2.0f);
    i64 y = (i64)(y_unit_scale * render_target_height);

    draw_text_with_backing(font, x, y, text, color);
}

void draw_text_with_backing(Dynamic_Font *font, i64 x, i64 y, String text, Vector4 color)
{
    auto ox = (i64)(font->character_height * 0.03f);
    auto oy = -ox;

    auto bg_color = Vector4(0, 0, 0, 0.6f * color.w);

    draw_prepared_text(font, x + ox, y + oy, bg_color);
    draw_prepared_text(font,      x,      y,    color);
}

void set_object_to_world_matrix(Vector3 pos, Quaternion ori, f32 scale)
{
    auto r = Matrix4(1.0f);
    set_rotation(&r, ori);

    auto m = Matrix4(1.0f);

    m[0][3] = pos.x;
    m[1][3] = pos.y;
    m[2][3] = pos.z;

    m[0][0] = scale;
    m[1][1] = scale;
    m[2][2] = scale;

    object_to_world_matrix = m * r;
    refresh_transform();
}

void draw_2d_entity(Entity *e, Vector4 *color_ptr = NULL)
{
    auto pos = e->position;
    auto b = e->boundary_radius;
    auto p0 = pos;
    auto p1 = pos;
    auto p2 = pos;
    auto p3 = pos;

    p0.x -= b;
    p0.y -= b;

    p1.x += b;
    p1.y -= b;

    p2.x += b;
    p2.y += b;

    p3.x -= b;
    p3.y += b;

    auto map = e->map;

    auto fcolor = Vector4(1, 1, 1, 1);
    if (color_ptr) fcolor = *color_ptr;

    auto icolor = argb_color(fcolor);

    if (map)
    {
        set_texture(String("diffuse_texture"), map);

        immediate_quad(p0, p1, p2, p3, icolor);
    }
    else
    {
        // Not supposed to get here.
        set_shader(shader_argb_no_texture);

        immediate_quad(p0, p1, p2, p3, icolor);

        // Change back to the normal thing
        set_shader(shader_argb_and_texture);
    }
}

void update_orientation(Entity *e)
{
    if (e->theta_target != e->theta_current)
    {
        auto magnitude = e->theta_target - e->theta_current;
        if (magnitude >  180) e->theta_current += 360;
        if (magnitude < -180) e->theta_current -= 360;

        auto dt = timez.current_dt;
        auto dtheta = dt * gameplay_visuals.general_turning_degrees_per_second;
        e->theta_current = move_toward(e->theta_current, e->theta_target, dtheta);
    }

    get_ori_from_rot(&e->orientation, Vector3(0, 0, 1), e->theta_current * (TAU / 360));
}

void mesh_draw(Triangle_Mesh *mesh, Vector3 position, f32 mesh_scale, Quaternion ori, Vector4 *scale_color = NULL, Vector4 *override_color = NULL)
{
    assert(mesh);

    set_shader(shader_basic_3d);

    // @Speed:
    // Quaternion mesh_correction;
    // get_ori_from_rot(&mesh_correction, Vector3(0, 0, 1), MESH_RENDER_THETA_OFFSET * (TAU / 360.0));
    // ori = ori * mesh_correction;

    auto r = Matrix4(1.0);
    set_rotation(&r, ori);

    auto m = Matrix4(1.0);
    m[3][0] = position.x;
    m[3][1] = position.y;
    m[3][2] = position.z;

    m[0][0] = mesh_scale;
    m[1][1] = mesh_scale;
    m[2][2] = mesh_scale;

    object_to_world_matrix = m * r;
    refresh_transform();

    glBindBuffer(GL_ARRAY_BUFFER, mesh->vertex_vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->index_vbo);
    set_vertex_format_to_XCNUU(current_shader);

    for (i32 list_i = 0; list_i < mesh->triangle_list_info.count; ++list_i)
    {
        auto list = mesh->triangle_list_info[list_i];
        if (!list.map) list.map = white_texture;

        set_texture(String("diffuse_texture"), list.map);

        auto render_mat = mesh->material_info[list.material_index];
        auto fcolor = render_mat.color;
        fcolor.w = 1.0f; // Just to make sure because I didn't make all the models.

        if (render_mat.flags & static_cast<u32>(Material_Flags::Dynamic_Substitute))
        {
            if (override_color) fcolor = *override_color;
        }

        if (scale_color)
        {
            fcolor.x *= scale_color->x;
            fcolor.y *= scale_color->y;
            fcolor.z *= scale_color->z;
            fcolor.w *= scale_color->w;
        }

        set_material_color(fcolor);

        auto offset = list.first_index * sizeof(mesh->index_array[0]);
        glDrawElements(GL_TRIANGLES, list.num_indices, GL_UNSIGNED_INT, reinterpret_cast<void*>(offset));
    }

    object_to_world_matrix = Matrix4(1.0);
    refresh_transform();
}

void draw_game_view_3d()
{
    // Render the scene to a offscreen buffer
    set_render_target(0, the_offscreen_buffer, the_depth_buffer);

    glClearColor(.05, .05, .05, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // ================================================================

    {
        //
        // Set up perspective
        //
        auto manager = get_entity_manager();
        auto camera = manager->camera;

        world_to_view_matrix   = camera.world_to_view_matrix;
        view_to_proj_matrix    = camera.view_to_proj_matrix;
        object_to_world_matrix = Matrix4(1.0);

        refresh_transform();

        set_shader(shader_argb_and_texture);

        // Draw the floor
        immediate_begin();
        for (auto it : manager->_by_floors)
        {
            draw_2d_entity(it->base);
        }
        immediate_flush();

        // Draw doors
        immediate_begin();
        for (auto it : manager->_by_doors)
        {
            draw_2d_entity(it->base);
        }
        immediate_flush();

        // Draw switches
        immediate_begin();
        for (auto it : manager->_by_switches)
        {
            Vector4 *color = NULL;

            if (it->base->use_override_color)
                color = &it->base->override_color;

            draw_2d_entity(it->base, color);
        }
        immediate_flush();

        set_shader(shader_basic_3d);

        // Draw walls
        immediate_begin();
        for (auto it : manager->_by_walls)
        {
            auto pos = it->base->position;
            auto map = it->base->map;
            auto b   = it->base->boundary_radius;

            draw_block_entity_at(pos, map, b);
        }
        immediate_flush();

        // Draw the guys
        immediate_begin();
        for (auto it : manager->_by_guys)
        {
            // @Temporary: Deprecate the old way of drawing a guy as a block!
            // We currently only have the fighter as a mesh, so convert this
            // to all meshes later this week!

            if (it->base->map) // :DeprecateMe: Old way of drawing 2D guys
            {
                auto pos = it->base->position;
                auto map = it->base->map;
                auto b   = it->base->boundary_radius;
            
                // If the guy is inactive, we draw him different
                Vector4 *color = NULL;
                if (!it->active)
                {
                    auto fcolor = Vector4(.3, .3, .3, 1);
                    color = &fcolor;
                }

                draw_block_entity_at(pos, map, b, false, false, false, color);
            }
            else if (it->base->mesh) // New way of drawing guys as meshes
            {
                auto mesh       = it->base->mesh;
                auto position   = it->base->visual_position;
                auto mesh_scale = 1.0f;

                update_orientation(it->base); // @Fixme: Move this into simulate()
                auto ori = it->base->orientation;
                
                mesh_draw(mesh, position, mesh_scale, ori);
            }
            else
            {
                assert(0 && "Guy should have contained a mesh!");
            }
        }

        mesh_draw(red_guy_mesh, Vector3(4, 1, 0), 1.0f, Quaternion(1, 0, 0, 0)); // :DeprecateMe Testing out the new model loader.

        immediate_flush();

        // Draw the rocks
        immediate_begin();
        for (auto it : manager->_by_rocks)
        {
            // If the rock is dead (example being a rock pushed into an opened gate, and then the gate
            // later got closed), then we don't draw it.

            if (it->base->dead) continue;

            auto pos = it->base->visual_position;
            auto map = it->base->map;
            auto b   = it->base->boundary_radius;

            draw_block_entity_at(pos, map, b);
        }
        immediate_flush();

        // Draw the gates
        immediate_begin();
        for (auto it : manager->_by_gates)
        {
            if (it->open) continue;

            auto pos = it->base->position;
            auto map = it->base->map;
            auto b   = it->base->boundary_radius;

            Vector4 *color = NULL;
            if (it->base->use_override_color)
                color = &it->base->override_color;

            draw_block_entity_at(pos, map, b, false, false, false, color);
        }
        immediate_flush();
    }

    // ================================================================
    
    rendering_2d_right_handed();

    // @Note: Draw the hud for the editor!
    // @Cleanup: Factor this with the hud.cpp
    if (program_mode == Program_Mode::EDITOR)
    {
        set_shader(shader_argb_and_texture);
        auto map = catalog_find(&texture_catalog, String("crosshair"));

        set_texture(String("diffuse_texture"), map);

        auto w = render_target_width;
        auto h = render_target_height;
        auto scale = 0.01f;
        auto b = scale * h;

        auto c = Vector2(w * 0.5, h * 0.5);

        auto p0 = c;
        auto p1 = c;
        auto p2 = c;
        auto p3 = c;

        p0.x -= b;
        p0.y -= b;

        p1.x += b;
        p1.y -= b;

        p2.x += b;
        p2.y += b;

        p3.x -= b;
        p3.y += b;

        Vector2 u0(0, 0);
        Vector2 u1(1, 0);
        Vector2 u2(1, 1);
        Vector2 u3(0, 1);

        immediate_begin();
        immediate_quad(p0, p1, p2, p3, u0, u1, u2, u3, 0xffffffff);
        immediate_flush();
    }

    // draw_editor();    // :DeprecateMe @Temporary Drawing the UI for testing the widget systems

    // @Note: Render that offscreen buffer as a quad onto the backbuffer
    set_render_target(0, the_back_buffer);
    rendering_2d_right_handed();

    glClearColor(.5, .5, .5, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    set_shader(shader_argb_and_texture);

    f32 back_buffer_width  = the_back_buffer->width;
    f32 back_buffer_height = the_back_buffer->height;

    f32 w  = (f32)(the_offscreen_buffer->width);
    f32 h  = (f32)(the_offscreen_buffer->height);
    f32 bx = floorf(0.5 * (back_buffer_width - the_offscreen_buffer->width));
    f32 by = floorf(0.5 * (back_buffer_height - the_offscreen_buffer->height));

    auto p0 = Vector2(bx,     by);
    auto p1 = Vector2(bx + w, by);
    auto p2 = Vector2(bx + w, by + h);
    auto p3 = Vector2(bx,     by + h);

    set_texture(String("diffuse_texture"), the_offscreen_buffer);

    immediate_begin();
    immediate_quad(p0, p1, p2, p3, 0xffffffff);
    immediate_flush();
}

void draw_generated_quads(Dynamic_Font *font, Vector4 color)
{
    rendering_2d_right_handed();
    set_shader(shader_text);

    // glBlendFunc(GL_SRC1_COLOR, GL_ONE_MINUS_SRC1_COLOR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY, 1.0); // @Investigate: What is anisotropy have to do with font rendering?
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_BLEND);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    GLuint last_texture_id = 0xffffffff;

    immediate_begin();

    for (auto quad : font->current_quads)
    {
        auto page = quad.glyph->page;
        auto map  = &page->texture;

        if (page->dirty)
        {
            // printf("! Should be generating texture again\n");
            page->dirty = false;
            auto bitmap = page->bitmap_data;

            // Regenerating the texture. Or should we not?
            {
                // if (map->id == 0xffffffff || !map->id)
                {
                    // printf("Generating a texture for font page\n");
                    glGenTextures(1, &map->id);
                    glBindTexture(GL_TEXTURE_2D, map->id);
                }

                map->width  = bitmap->width;
                map->height = bitmap->height;
                map->data   = bitmap;
                map->dirty  = true;
            }
        }

        if (map->id != last_texture_id)
        {
            // @Speed
            // This will cause a flush for every call to draw_text.
            // But if we don't do this then we won't set the texture.
            // Need to refactor the text rendering code so that we don't have to deal with this
            immediate_flush();
            last_texture_id = map->id;
            set_texture(String("diffuse_texture"), map);
        }

        immediate_letter_quad(quad, color);
        // immediate_flush();
    }

    immediate_flush();
}

void draw_prepared_text(Dynamic_Font *font, i64 x, i64 y, Vector4 color)
{
    generate_quads_for_prepared_text(font, x, y);
    draw_generated_quads(font, color);
}

i64 draw_text(Dynamic_Font *font, i64 x, i64 y, String text, Vector4 color)
{
    auto width = prepare_text(font, text);
    draw_prepared_text(font, x, y, color);

    return width;
}

void draw_quad(Vector2 p0, Vector2 p1, Vector2 p2, Vector2 p3, Vector4 fcolor) // @Speed:
{
    rendering_2d_right_handed();

    immediate_begin();

    auto color = argb_color(fcolor);
    immediate_quad(p0, p1, p2, p3, color);

    immediate_flush();
}

void draw_quad(Vector2 p0, Vector2 p1, Vector2 p2, Vector2 p3, Vector4 c0, Vector4 c1, Vector4 c2, Vector4 c3)  // Not really efficient because the quads are not batched together.
{
    rendering_2d_right_handed();

    immediate_begin();

    immediate_quad(p0, p1, p2, p3, c0, c1, c2, c3);

    immediate_flush();
}

void draw_triangle(Vector2 p0, Vector2 p1, Vector2 p2, Vector4 fcolor) // Not really efficient because the triangles are not batched together.
{
    rendering_2d_right_handed();

    immediate_begin();

    auto color = argb_color(fcolor);
    immediate_triangle(p0, p1, p2, color);

    immediate_flush();
}

void draw_triangle(Vector2 p0, Vector2 p1, Vector2 p2, Vector4 c0, Vector4 c1, Vector4 c2) // @Speed:
{
    rendering_2d_right_handed();

    immediate_begin();

    auto color0 = argb_color(c0);
    auto color1 = argb_color(c2);
    auto color2 = argb_color(c2);
    immediate_triangle(p0, p1, p2, color0, color1, color2);

    immediate_flush();
}

