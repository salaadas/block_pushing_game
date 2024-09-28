#include "draw.h"

#include "main.h"
#include "opengl.h"
#include "time_info.h"
#include "file_utils.h"
#include "camera.h"
#include "mesh_catalog.h"
#include "sokoban.h"
#include "editor.h"
#include "debug_draw.h"
#include "player_control.h" // For axis_*

const f32 MESH_RENDER_THETA_OFFSET = 90.0f; // The mesh in the modeling software is facing to the negative-y. So we offset them to start at the positive-x direction.... Ughhh @Fixme: This causes a lot of hacks.

Shader *shader_argb_no_texture;
Shader *shader_argb_and_texture;
Shader *shader_text;
Shader *shader_basic_3d;
Shader *shader_skinned_3d;
Shader *shader_selection;
Shader *shader_hdr_to_ldr;
Shader *shader_create_shadow_map;

bool XXX_hack_drawing_selection = false;

#include "animation_player.h"

Triangle_Mesh *sponza_scene = NULL; // :DeprecateMe
Texture_Map *red_curtain; // :DeprecateMe
Texture_Map *non_compressed_texture; // :DeprecateMe
Texture_Map *brick_wall; // :DeprecateMe
Texture_Map *brick_wall_normal;

constexpr f32 Z_NEAR_DEFAULT = 0.1f;
constexpr f32 Z_FAR_DEFAULT  = 200.0f;

Matrix4 matrix_create_shadow_map;

struct Sun
{
    Vector3    position = Vector3(5.0f, 2.0f, 10.0f);
    Quaternion orientation;
    Vector4    color = Vector4(1.0f, 0.8f, 0.32f, 1.0f);
    f32        intensity = 1.0f;

    Vector3 direction; // For convenience, this is calculated from the position and orientation.

    f32 z_near = 0.1f;
    f32 z_far  = 10.5f;
    // f32 z_far  = 15f;

    f32 frustum_radius = 10.0f;
};

Sun sun = {}; // @Cleanup: Put this inside the entity_manager?

Render_Type current_render_type = Render_Type::MAIN_VIEW;

#define cat_find(catalog, name) catalog_find(&catalog, String(name));

void init_shaders()
{
    shader_argb_no_texture  = cat_find(shader_catalog, "argb_no_texture");  assert(shader_argb_no_texture);
    shader_argb_and_texture = cat_find(shader_catalog, "argb_and_texture"); assert(shader_argb_and_texture);
    shader_text             = cat_find(shader_catalog, "text");             assert(shader_text);
    shader_basic_3d         = cat_find(shader_catalog, "basic_3d");         assert(shader_basic_3d);
    shader_skinned_3d       = cat_find(shader_catalog, "skinned_3d");       assert(shader_skinned_3d);
    shader_selection        = cat_find(shader_catalog, "selection");        assert(shader_selection);
    shader_hdr_to_ldr       = cat_find(shader_catalog, "hdr_to_ldr");       assert(shader_hdr_to_ldr);
    shader_create_shadow_map = cat_find(shader_catalog, "create_shadow_map"); assert(shader_create_shadow_map);

    shader_argb_no_texture->backface_cull  = false;
    shader_argb_and_texture->backface_cull = false;

    { // :DeprecateMe: Testing out the new material system.
       sponza_scene = cat_find(mesh_catalog, "sponza"); assert(sponza_scene);
        // sponza_scene = cat_find(mesh_catalog, "sponza_png"); assert(sponza_scene);

        // Testing out loading compressed texture map.
        // red_curtain.full_path = String("data/textures/flipped-red-curtain.dds");
        // auto success = load_dds_texture_helper(&red_curtain);
        // printf("Did I load the red curtain? %d\n", success);

        red_curtain = catalog_find(&texture_catalog, String("flipped-red-curtain"));
        non_compressed_texture = catalog_find(&texture_catalog, String("non-compressed-holder"));
    }

    // Doing the sun stuff. We will probably want to do this every frame if we decide to
    // manipulate the sun position/orientation and stuff like that from the editor.
    {
        Quaternion q1;
        get_ori_from_rot(&q1, Vector3(0, 1, 0), TAU * 1 / 2.5f);

        Quaternion q2;
        get_ori_from_rot(&q2, Vector3(1, 0, 0), TAU * 75.0f / 360.0f);
        sun.orientation = q1 * q2;

        auto look_at  = sun.position + rotate(axis_forward, sun.orientation);
        sun.direction = look_at - sun.position;
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

    auto condition = (shader == shader_basic_3d) || (shader == shader_skinned_3d) || (shader == shader_selection);
    if (!condition) return;

    auto loc = glGetUniformLocation(shader->program, "material_color");
    if (loc < 0)
    {
        // logprint("set_material_color", "Could not find the material color for current shader '%s'.\n", temp_c_string(shader->name));
        DumpGLErrors("set_material_color");

        return;
    }

    glUniform4fv(loc, 1, (f32*)&color);
}

// @Hack: This is to set the shadow mapping parameters stuff. Although there are some parameters
// like the camera position we also use during the render of the MAIN_VIEW too....
void prepare_uniforms_for_shadow_mapping(Shader *shader)
{
    auto loc = glGetUniformLocation(shader->program, "object_to_world_matrix");
    glUniformMatrix4fv(loc, 1, GL_FALSE, &(object_to_world_matrix[0][0]));

    loc = glGetUniformLocation(shader->program, "matrix_create_shadow_map");
    glUniformMatrix4fv(loc, 1, GL_FALSE, &(matrix_create_shadow_map[0][0]));

    // Important: Make sure that the sun stuff is initialized first!!!!
    loc = glGetUniformLocation(shader->program, "sun_direction");
    glUniform3fv(loc, 1, (f32*)&sun.direction);

    loc = glGetUniformLocation(shader->program, "sun_color");
    glUniform4fv(loc, 1, (f32*)&sun.color);

    loc = glGetUniformLocation(shader->program, "sun_position");
    glUniform3fv(loc, 1, (f32*)&sun.position);

    auto manager = get_entity_manager(); // @Cleanup: Should pass an entity manager here.
    auto camera  = &manager->camera;

    loc = glGetUniformLocation(shader->program, "camera_position");
    glUniform3fv(loc, 1, (f32*)&camera->position);

    set_texture(String("shadow_map_texture"), shadow_map_depth);
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

    if (current_render_type == Render_Type::MAIN_VIEW) set_shader(shader_basic_3d);
    else                                               set_shader(shader_create_shadow_map);

    {
        auto shader = current_shader;
        prepare_uniforms_for_shadow_mapping(shader);
    }

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

void draw_centered(Dynamic_Font *font, f32 y_unit_scale, String text, Vector4 color)
{
    auto width = prepare_text(font, text);

    i64 x = (i64)((render_target_width - width) / 2.0f);
    i64 y = (i64)(y_unit_scale * render_target_height);

    draw_prepared_text_with_backing(font, x, y, color);
}

// This doesn't take 'text' because it assumes the prepared text is what we want to draw.
void draw_prepared_text_with_backing(Dynamic_Font *font, i64 x, i64 y, Vector4 color)
{
    auto ox = (i64)(font->character_height * 0.05f);
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

    m[3][0] = pos.x;
    m[3][1] = pos.y;
    m[3][2] = pos.z;

    m[0][0] = scale;
    m[1][1] = scale;
    m[2][2] = scale;

    object_to_world_matrix = m * r;
    refresh_transform();
}

void draw_2d_entity(Entity *e, Vector4 *color_ptr = NULL)
{
    auto pos = e->position;
    auto b = e->bounding_radius;
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

    if (current_render_type == Render_Type::MAIN_VIEW) set_shader(shader_basic_3d);
    else                                               set_shader(shader_create_shadow_map);

    {
        auto shader = current_shader;
        prepare_uniforms_for_shadow_mapping(shader);
    }

    if (map)
    {
        set_texture(String("diffuse_texture"), map);

        set_material_color(fcolor);
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

void set_matrix_for_entities(Entity_Manager *manager, Vector2 offset)
{
    if (current_render_type == Render_Type::SHADOW_MAP)
    {
        world_to_shadow_map_matrix = matrix_create_shadow_map;
        return;
    }

    f32 w = render_target_width;
    f32 h = render_target_height;
    if (h < 1) h = 1;

    auto vFOV   = 40.f * (9/16.0f); // TAU * .25f * (9/16.0f); @Fixme: Turn this back on when we are done with the camera stuff.
    // auto vFOV   = 70.f * (9/16.0f); // TAU * .25f * (9/16.0f);

    auto z_near = Z_NEAR_DEFAULT;
    auto z_far  = Z_FAR_DEFAULT;

    // auto control = get_camera_control(manager);
    // if (control)
    // {
    //     vFOV   = (TAU / 360.0f) * control->fov_vertical;
    //     z_near = control->z_near;
    //     z_far  = control->z_far;
    // }

    auto viewpoint   = manager->camera.position;
    auto orientation = manager->camera.orientation;

    // @Temporary @Cleanup: Get rid of the look_at matrix;
    auto look_at = viewpoint + rotate(axis_forward, orientation);

    viewpoint.x -= offset.x;
    viewpoint.y -= offset.y;
    look_at.x   -= offset.x;
    look_at.y   -= offset.y;

    auto up = axis_up;
    // @Cleanup: We are converting an orientation to a look_at, ...
    // instead let's just do the matrix directly from viewpoint and orientation.
    auto view = make_look_at_matrix(viewpoint, look_at - viewpoint, up);

//    auto proj = make_projection_matrix(vFOV, w/h, z_near, z_far, 0, 0);  // @Investigate: What are the 2 last arguments for?
    auto proj = make_projection_matrix(z_near, z_far, w/h, vFOV);

    manager->camera.world_to_view_matrix = view;
    manager->camera.view_to_proj_matrix  = proj;
    manager->camera.z_near               = z_near;
    manager->camera.z_far                = z_far;
    manager->camera.fov_vertical         = vFOV;

    manager->camera.forward_vector = unit_vector(look_at - viewpoint);

    view_to_proj_matrix    = proj;
    world_to_view_matrix   = view;
    object_to_world_matrix = Matrix4(1.0f);

    refresh_transform();
}

void draw_mesh_entity(Triangle_Mesh *mesh, Vector3 position, f32 mesh_scale, Quaternion ori, Vector4 *scale_color = NULL, Vector4 *override_color = NULL, Animation_Player *player = NULL)
{
    assert(mesh);

    // @Cleanup: This procedure is kinda a "give me a mesh and I will figure out how to draw",
    // so we should think about making a struct with the settings and stuff to pass to this
    // if we need to do more later (when doing PBR materials).
    if (player)
    {
        if (XXX_hack_drawing_selection) set_shader(shader_selection);
        else                            set_shader(shader_skinned_3d);
    }
    else
    {
        set_shader(shader_basic_3d);   // No skinning.
    }

    // I think we should move this to the draw_guy_at since only guys are concerns with orientations....
    Quaternion mesh_correction;
    get_ori_from_rot(&mesh_correction, Vector3(0, 0, 1), MESH_RENDER_THETA_OFFSET * (TAU / 360.0));
    ori = ori * mesh_correction;

    set_object_to_world_matrix(position, ori, mesh_scale);

    // Setting the uniforms
    // @Temporary: Maybe we only want to do shadow for certain entity types only?
    {
        auto shader = current_shader;
        prepare_uniforms_for_shadow_mapping(shader);
    }

    glBindBuffer(GL_ARRAY_BUFFER,         mesh->vertex_vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->index_vbo);

    if (!player)
    {
        set_vertex_format_to_XCNUU(current_shader);
    }
    else
    {
        set_vertex_format_to_XCNUUS(current_shader); // @Cleanup: Do something about this.... We should set the shader according to the Render Material inside the mesh.

        // Blend matrices or skinning matrices that we got from skin_mesh().
        auto blend_matrices_ptr = &(player->output_matrices[0][0][0]);
        glUniformMatrix4fv(current_shader->blend_matrices_loc, player->output_matrices.count, GL_FALSE, blend_matrices_ptr);
    }

    for (i32 list_i = 0; list_i < mesh->triangle_list_info.count; ++list_i)
    {
        auto list = mesh->triangle_list_info[list_i];

        auto render_mat = mesh->material_info[list.material_index];

        auto albedo_map = render_mat.albedo_2024;
        if (!albedo_map) albedo_map = white_texture;

        set_texture(String("diffuse_texture"), albedo_map);

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

        // Render stuff in immediate mode to debug skeletal animations.
/*
        {
            auto vertices = mesh->vertices;
            // if (mesh->skinned_vertices.count)
            // {
            //     // @Cleanup: Make a assignment operator for RArr to SArr?
            //     vertices.count = mesh->skinned_vertices.count;
            //     vertices.data  = mesh->skinned_vertices.data;
            // }

            immediate_begin();
            for (i32 i = list.first_index; i < (list.first_index + list.num_indices); i += 3)
            {
                auto i0 = mesh->index_array[i + 0];
                auto i1 = mesh->index_array[i + 1];
                auto i2 = mesh->index_array[i + 2];

                auto p0 = vertices[i0];
                auto p1 = vertices[i1];
                auto p2 = vertices[i2];

                auto n0 = mesh->vertex_frames[i0].normal;
                auto n1 = mesh->vertex_frames[i1].normal;
                auto n2 = mesh->vertex_frames[i2].normal;

                auto uv0 = mesh->uvs[i0];
                auto uv1 = mesh->uvs[i1];
                auto uv2 = mesh->uvs[i2];

                auto c = argb_color(fcolor);
                immediate_vertex(p0, c, n0, uv0);
                immediate_vertex(p1, c, n1, uv1);
                immediate_vertex(p2, c, n2, uv2);

                {
                    auto v4 = Vector4(p0.x, p0.y, p0.z, 1);
                    auto t4 = object_to_proj_matrix * v4;
                    auto zw = t4.z / t4.w;
                    if ((zw < -1) || (zw > 1))
                    {
                        // printf("zw is out of range: %f, v4 {%f %f %f %f}, t4 {%f %f %f %f}\n", zw, v4.x, v4.y, v4.z, v4.w, t4.x, t4.y, t4.z, t4.w);
                    }
                }
            }
            immediate_flush();
        }
*/
    }

    object_to_world_matrix = Matrix4(1.0);
    refresh_transform();
}

// void draw_guy_at(Guy *guy, bool dimmed = false, bool dead = false, Vector4 *color_ptr = NULL)
void draw_guy_at(Guy *guy, bool dimmed, bool dead, Vector4 *color_ptr)
{
    // if (guy->base->entity_flags & ENTITY_INVISIBLE) return;

    auto mesh = guy->base->mesh;
    if (!mesh) return;

    Vector4 color; // This is scale color.
    if (dimmed || dead)
    {
        auto k = 0.1f;
        if (color_ptr) color = Vector4(color_ptr->x * k, color_ptr->y * k, color_ptr->z * k, color_ptr->w);
        else           color = Vector4(k, k, k, 1.0f);
    }
    else
    {
        if (color_ptr) color = Vector4(color_ptr->x, color_ptr->y, color_ptr->z, color_ptr->w);
        else           color = Vector4(1, 1, 1, 1);
    }

    Vector4 *override_color;
    if (guy->base->use_override_color)
    {
        override_color = &guy->base->override_color;
    }

    auto scale = guy->base->scale;
    auto player = guy->base->animation_player;
    draw_mesh_entity(mesh, guy->base->visual_position, scale, guy->base->orientation, &color, override_color, player);
}

void draw_game_view_3d()
{
    // Render the scene to the shadow map buffer.
    current_render_type = Render_Type::SHADOW_MAP;

    // auto sponza_position = Vector3(6, 5, 0); // :DeprecateMe @Temporary
    // f32 sponza_scale = .002f;

    {
        auto old_multisampling = multisampling; // @Hack: We should make multisampling be a flag for Texture_Map!!
        multisampling = false;

        set_render_target(0, shadow_map_buffer, shadow_map_depth);

        multisampling = old_multisampling;

        glClearColor(1, 1, 1, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        auto manager = get_entity_manager();

        // Setting the matrix for creating the shadow map.
        {
            //
            // Making the orthographic projection matrix.
            //
            auto r = sun.frustum_radius;
            auto shadow_proj_matrix = glm::ortho(-r, r, -r, r, sun.z_near, sun.z_far);

            //
            // Making the view matrix.
            //
            // @Temporary @Cleanup: Get rid of the look_at matrix;
            auto up = Vector3(0, 0, 1);
            auto shadow_view_matrix = make_look_at_matrix(sun.position, sun.direction, up);

            // Testing shadow map that uses our camera as a light source.
            matrix_create_shadow_map = shadow_proj_matrix * shadow_view_matrix;
        }

        set_matrix_for_entities(manager, Vector2(0, 0));

        // draw_mesh_entity(sponza_scene, sponza_position, sponza_scale, Quaternion(1, 0, 0, 0));

        // Draw the floor
        immediate_begin();
        for (auto it : manager->_by_floors) draw_2d_entity(it->base);
        immediate_flush();

        // Draw doors
        immediate_begin();
        for (auto it : manager->_by_doors) draw_2d_entity(it->base);
        immediate_flush();

        // Draw switches
        immediate_begin();
        for (auto it : manager->_by_switches)
        {
            Vector4 *color = NULL;

            if (it->base->use_override_color) color = &it->base->override_color;

            draw_2d_entity(it->base, color);
            immediate_flush();
        }

        // Draw walls
        immediate_begin();
        for (auto it : manager->_by_walls)
        {
            auto pos = it->base->position;
            auto map = it->base->map;
            auto b   = it->base->bounding_radius;

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
                auto pos = it->base->visual_position;
                auto map = it->base->map;
                auto b   = it->base->bounding_radius;
            
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
                update_orientation(it->base); // @Fixme: Move this into simulate()

                auto dimmed = true;
                if (it->active) dimmed = false;

                draw_guy_at(it, dimmed);
            }
            else
            {
                assert(0 && "Guy should have contained a mesh or at least a texture!");
            }
        }
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
            auto b   = it->base->bounding_radius;

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
            auto b   = it->base->bounding_radius;

            Vector4 *color = NULL;
            if (it->base->use_override_color) color = &it->base->override_color;
            draw_block_entity_at(pos, map, b, false, false, false, color);

            immediate_flush();
        }
    }

    // ================================================================

    // Render the scene to the offscreen buffer
    current_render_type = Render_Type::MAIN_VIEW;

    {
        set_render_target(0, the_offscreen_buffer, the_depth_buffer);
        glClearColor(.003, .003, .003, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        //
        // Set up the matrices.
        //
        auto manager = get_entity_manager();
        set_matrix_for_entities(manager, Vector2(0, 0));

        // draw_mesh_entity(sponza_scene, sponza_position, sponza_scale, Quaternion(1, 0, 0, 0));

        // Draw the floor
        immediate_begin();
        for (auto it : manager->_by_floors) draw_2d_entity(it->base);
        immediate_flush();

        // Draw doors
        immediate_begin();
        for (auto it : manager->_by_doors) draw_2d_entity(it->base);
        immediate_flush();

        // Draw switches
        immediate_begin();
        for (auto it : manager->_by_switches)
        {
            Vector4 *color = NULL;

            if (it->base->use_override_color) color = &it->base->override_color;

            draw_2d_entity(it->base, color);
            immediate_flush();
        }

        // Draw walls
        immediate_begin();
        for (auto it : manager->_by_walls)
        {
            auto pos = it->base->position;
            auto map = it->base->map;
            auto b   = it->base->bounding_radius;

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
                auto pos = it->base->visual_position;
                auto map = it->base->map;
                auto b   = it->base->bounding_radius;
            
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
                update_orientation(it->base); // @Fixme: Move this into simulate()

                auto dimmed = true;
                if (it->active) dimmed = false;

                draw_guy_at(it, dimmed);
            }
            else
            {
                assert(0 && "Guy should have contained a mesh or at least a texture!");
            }
        }
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
            auto b   = it->base->bounding_radius;

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
            auto b   = it->base->bounding_radius;

            Vector4 *color = NULL;
            if (it->base->use_override_color) color = &it->base->override_color;
            draw_block_entity_at(pos, map, b, false, false, false, color);

            immediate_flush();
        }
    }

    // ================================================================

    // @Note: drawing this up here before turning to the back buffer because we actually want
    // to position the animation HUD on top of the world space.
    // This also means that we have to suffer with the sRGB color....
    draw_debug_animation_view();

    set_render_target(0, the_back_buffer);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);

    rendering_2d_right_handed();

    // Resolve to LDR. Everything after here uses the normal RGB color space.
    {
        set_shader(shader_hdr_to_ldr);

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

        // set_texture(String("diffuse_texture"), the_offscreen_buffer); // This version is for the non-multisampled offscreen_buffer. @Note: Don't delete me, keep me around for future purposes.

        set_texture(String("diffuse_texture"), the_ldr_buffer); // This is used as the intermediate thing.

        immediate_begin();
        immediate_quad(p0, p1, p2, p3, 0xffffffff);
        immediate_flush();
    }

    draw_debug_shadow_map_view();

    // :DeprecateMe drawing the images for the texture compression test.
/*
    {
        set_shader(shader_argb_and_texture);
        set_texture(String("diffuse_texture"), red_curtain);

        // auto w = 800;
        // auto h = 600;
        // auto w = 400;
        // auto h = 300;

        auto w = 200;
        auto h = 200;

        auto p0 = Vector2(100,     100);
        auto p1 = Vector2(100 + w, 100);
        auto p2 = Vector2(100 + w, 100 + h);
        auto p3 = Vector2(100,     100 + h);

        Vector2 u0(0, 0);
        Vector2 u1(1, 0);
        Vector2 u2(1, 1);
        Vector2 u3(0, 1);

        immediate_begin();
        immediate_quad(p0, p1, p2, p3, u0, u1, u2, u3, 0xffffffff);
        immediate_flush();

        set_texture(String("diffuse_texture"), non_compressed_texture);
        p0 = p1 + Vector2(w, 0);
        p1 = p0 + Vector2(w, 0);
        p2 = p1 + Vector2(0, h);
        p3 = p0 + Vector2(0, h);
        immediate_begin();
        immediate_quad(p0, p1, p2, p3, u0, u1, u2, u3, 0xffffffff);
        immediate_flush();
    }
*/

    // @Note: Draw the hud for the editor!
    // @Cleanup: Factor this with the hud.cpp
    if (program_mode == Program_Mode::EDITOR)
    {
        rendering_2d_right_handed();
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

                map->format = bitmap->XXX_format;
                update_texture_from_bitmap(map, bitmap);
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

