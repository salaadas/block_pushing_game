#include "debug_draw.h"
#include "animation_hud.h"

#include "draw.h"
#include "opengl.h"

bool debug_draw_animation;
bool debug_draw_shadow_map;

void draw_debug_animation_view()
{
    if (debug_draw_animation) draw_animation_hud();
}

void draw_debug_shadow_map_view()
{
    if (!debug_draw_shadow_map) return;

    rendering_2d_right_handed_unit_scale();

    set_shader(shader_argb_and_texture);
    set_texture(String("diffuse_texture"), shadow_map_depth);

    auto w = 1.0f / 3.0f;
    auto m = w / 20;

    auto p0 = Vector2(1 - m - w, m);
    auto p1 = p0 + Vector2(w, 0);
    auto p2 = p0 + Vector2(w, w);
    auto p3 = p0 + Vector2(0, w);

    Vector2 u0(0, 0);
    Vector2 u1(1, 0);
    Vector2 u2(1, 1);
    Vector2 u3(0, 1);

    immediate_begin();
    immediate_quad(p0, p1, p2, p3, u0, u1, u2, u3, 0xffffffff);
    immediate_flush();
}
