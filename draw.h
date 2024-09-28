#pragma once

#include "common.h"
#include "font.h"
#include "catalog.h"
#include "texture_catalog.h"
#include "shader_catalog.h"

extern Shader *shader_argb_no_texture;
extern Shader *shader_argb_and_texture;
extern Shader *shader_text;
extern Shader *shader_skinned_3d;
extern Shader *shader_selection;

extern bool XXX_hack_drawing_selection;

extern const f32 MESH_RENDER_THETA_OFFSET; // @Fixme @Hack: This is right now we are not exporting the models from blender in the correct facing way.

enum class Render_Type
{
    MAIN_VIEW = 0,
    SHADOW_MAP
};

extern Render_Type current_render_type;

void init_shaders();
void rendering_2d_right_handed();
void rendering_2d_right_handed_unit_scale();

void draw_game_view_3d();

void draw_prepared_text(Dynamic_Font *font, i64 x, i64 y, Vector4 color);
i64  draw_text(Dynamic_Font *font, i64 x, i64 y, String text, Vector4 color);
void draw_generated_quads(Dynamic_Font *font, Vector4 color);
void draw_letter_quad(Font_Quad q, Vector4 color);
void draw_centered(Dynamic_Font *font, f32 y_unit_scale, String text, Vector4 color);
void draw_prepared_text_with_backing(Dynamic_Font *font, i64 x, i64 y, Vector4 color);

void draw_quad(Vector2 p0, Vector2 p1, Vector2 p2, Vector2 p3, Vector4 fcolor);
void draw_quad(Vector2 p0, Vector2 p1, Vector2 p2, Vector2 p3, Vector4 c0, Vector4 c1, Vector4 c2, Vector4 c3);
void draw_triangle(Vector2 p0, Vector2 p1, Vector2 p2, Vector4 fcolor);
void draw_triangle(Vector2 p0, Vector2 p1, Vector2 p2, Vector4 c0, Vector4 c1, Vector4 c2);

struct Entity_Manager;
void set_matrix_for_entities(Entity_Manager *manager, Vector2 offset);

void set_object_to_world_matrix(Vector3 pos, Quaternion ori, f32 scale);

struct Guy;
void draw_guy_at(Guy *guy, bool dimmed = false, bool dead = false, Vector4 *color_ptr = NULL);
