#pragma once

#include "common.h"
#include "font.h"
#include "catalog.h"
#include "texture_catalog.h"
#include "shader_catalog.h"

extern Shader *shader_argb_no_texture;
extern Shader *shader_argb_and_texture;
extern Shader *shader_text;

void init_shaders();
void rendering_2d_right_handed();
void rendering_2d_right_handed_unit_scale();

void draw_game_view_3d();

void draw_prepared_text(Dynamic_Font *font, i64 x, i64 y, Vector4 color);
i64  draw_text(Dynamic_Font *font, i64 x, i64 y, String text, Vector4 color);
void draw_generated_quads(Dynamic_Font *font, Vector4 color);
void draw_letter_quad(Font_Quad q, Vector4 color);
void draw_centered(Dynamic_Font *font, f32 y_unit_scale, String text, Vector4 color);

void draw_quad(Vector2 p0, Vector2 p1, Vector2 p2, Vector2 p3, Vector4 fcolor);
void draw_quad(Vector2 p0, Vector2 p1, Vector2 p2, Vector2 p3, Vector4 c0, Vector4 c1, Vector4 c2, Vector4 c3);
void draw_triangle(Vector2 p0, Vector2 p1, Vector2 p2, Vector4 fcolor);
void draw_triangle(Vector2 p0, Vector2 p1, Vector2 p2, Vector4 c0, Vector4 c1, Vector4 c2);
