#include "color_picker.h"

#include "ui_states.h"
#include "opengl.h"
#include "draw.h"
#include "events.h"

enum class Slider_Index
{
    Hue_Slider = 0,
    Hue_Slider_Zoomed,
    Saturation_Slider,
    Value_Slider, // How white is the color.
    Count
};

constexpr i32 MAX_STASHED_COLORS = 10;

Vector3 hsl_to_rgb(Vector3 hsl);
Vector3 rgb_to_hsl(Vector3 rgb);
Vector3 hsv_to_rgb(Vector3 hsv);
Vector4 hsv_to_argb(Vector3 hsv);
Vector3 make_hsv(f32 h, Vector2 p, Vector2 s_vector, Vector2 v_vector);

void draw_bordered_triangle(Vector2 apex, f32 apex_height, f32 base_half_width, f32 triangle_border_width, Vector2 dir, Vector4 color, Vector4 border_color);

f32 get_inscribed_square_intersector_length(f32 x, f32 y);
Vector2 circle_strech(f32 x, f32 y);
Rect expand(Rect r, f32 amount);

void update_animation_values(Color_Picker_State *state, f32 dt);
void update_ring_point(Color_Picker_State *state, Vector2 center, f32 r0, f32 r1);
void update_disc_point(Color_Picker_State *state, Vector2 center, f32 radius);

void set_current_color_hsl(Color_Picker_State *state, Vector3 hsl, bool update_zoom_hue = true);
void set_current_color_rgb(Color_Picker_State *state, Color_Picker_Theme *theme, Vector3 rgb, bool update_hsl = true, bool update_disc_point = false);

void stash_color(Color_Picker_State *state, Vector3 color);
void draw_hsl_readout(Rect r, Label_Theme *theme, Color_Picker_State *state, f32 line_height);
void draw_hsl_sliders(Rect area, Color_Picker_Theme *theme, Color_Picker_State *state);
void draw_hsl_circle(Rect r, Color_Picker_Theme *theme, Color_Picker_State *state, f32 circle_r0, f32 circle_r1);

void get_watch_colors(Color_Picker_Theme *theme, Vector3 rgb,
                      Vector4 *rgba, Vector4 *over, Vector4 *pressed, Vector4 *text)
{
    // auto white_zone = values_are_close(rgb, Vector3(1, 1, 1), Vector3(.2f));
    auto luminance = glm::dot(rgb, Vector3(.299, .587, .114));
    auto white_zone = (luminance > .65f);

    if (white_zone)
    {
        auto k = .85f;
        auto c = rgb * k;
        auto d = .15f;

        *over    = darken(Vector4(c.x, c.y, c.z, 1), d);
        *pressed = darken(Vector4(c.x, c.y, c.z, 1), .3f); // @Theme
        *text    = Vector4(0, 0, 0, 1); // @Theme
    }
    else
    {
        auto k = 1.25f;
        auto c = rgb * k;
        auto w = .15f;

        Clamp(&c.x, 0.0f, 1.0f);
        Clamp(&c.y, 0.0f, 1.0f);
        Clamp(&c.z, 0.0f, 1.0f);

        *over    = whiten(Vector4(c.x, c.y, c.z, 1), w);
        *pressed = whiten(Vector4(c.x, c.y, c.z, 1), .3f); // @Theme
        *text    = Vector4(1, 1, 1, 1); // @Theme
    }

    *rgba = Vector4(rgb.x, rgb.y, rgb.z, 1);
}

bool /*applied*/ color_picker(Rect r, Vector3 *input_and_output_color_rgb, Color_Picker_Theme *theme, i64 identifier, Source_Location loc)
{
    if (!theme) theme = &default_overall_theme.color_picker_theme;

    auto ipad_x = floorf(.5f + r.w * theme->horizontal_padding);
    auto ipad_y = floorf(.5f + r.w * theme->vertical_padding);

    auto [top, non_top]           = cut_top(r, ipad_y);
    auto [left_margin, remainder] = cut_left(non_top, ipad_x);
    auto [right_margin, middle]   = cut_right(remainder, ipad_x);

    auto istashed_color_width = floorf(.5f + r.w * theme->stashes_color_width);
    auto [stashed_color_area, left2] = cut_right(middle, istashed_color_width);

    auto [junk, circle_plus_extra] = cut_right(left2, floorf(.5f + r.w * theme->margin_between_color_input_and_stashed_colors));

    auto circle_area_width  = circle_plus_extra.w;
    auto circle_area_height = circle_area_width;

    auto [circle_area, junk2]   = cut_top(circle_plus_extra, circle_area_height);
    auto [junk3, controls_area] = cut_top(middle, circle_area_height + ipad_y);

    {
        // Draw the background;
        // Vector2 p0, p1, p2, p3;
        // get_quad(r, &p0, &p1, &p2, &p3);
        set_shader(shader_argb_no_texture);
        // draw_quad(p0, p1, p2, p3, theme->background_color);
        rounded_rectangle(r, theme->background_shape, theme->background_color);
        immediate_flush();
    }

    auto hash = ui_get_hash(loc, identifier);
    bool created = false;
    auto state = find_or_create_state<u64, Color_Picker_State>(&table_color_picker, hash, &created);
    defer { stop_using_state(&state->widget); };

    auto circle_r0 = circle_area.w * (theme->inner_circle_diameter) * .5f;
    auto circle_r1 = circle_area.w * (theme->outer_circle_diameter) * .5f;

    if (created)
    {
        state->original_rgb_color = *input_and_output_color_rgb;
        set_current_color_rgb(state, theme, state->original_rgb_color);

        for (auto it : theme->initial_stashed_colors) array_add(&state->stashed_colors, it);
    }

    auto status = get_status_flags(circle_area);
    if ((status & Status_Flags::OVER) && (mouse_button_right_state & KSTATE_START))
    {
        state->color_pick_mode = static_cast<Color_Pick_Mode>(static_cast<u32>(state->color_pick_mode) + 1);
        if (state->color_pick_mode >= Color_Pick_Mode::Count) state->color_pick_mode = static_cast<Color_Pick_Mode>(0);
    }

    auto dt = ui_current_dt;
    update_animation_values(state, dt); // @Note: We could have faster responsiveness if we do a separate update then draw cycle, or if we do special bumping of variables when e.g. button presses happen...

    switch (state->color_pick_mode)
    {
        case Color_Pick_Mode::Hsl_Circle: {
            draw_hsl_circle(circle_area, theme, state, circle_r0, circle_r1);
        } break;
        case Color_Pick_Mode::Hsv_Sliders: {
            draw_hsl_sliders(circle_area, theme, state);
        } break;
    }

    auto button_height = controls_area.h * .22f; // @Theme: Hardcoded
    auto line_height   = button_height;
    auto [_buttons, text_area] = cut_top(controls_area, button_height * 2);
    draw_hsl_readout(text_area, &theme->text_theme, state, line_height);

    immediate_flush();

    //
    // Draw output swatches...
    //
    
    // auto ypad = 0.0f; // @Theme padding

    auto [buttons_area, controls_lower] = cut_top(controls_area, controls_area.h * .2f); // @Theme @Hardcoded

    auto swatch_y0 = buttons_area.y;
    auto applied = false; // This is our return value.

    {
        auto bottom_button_width = (buttons_area.w - 3 * ipad_x) / 2.f;

        auto x0a = buttons_area.x;
        auto x0b = x0a + bottom_button_width;
        auto x2a = x0b + ipad_x;
        auto x2b = x2a + bottom_button_width;

        Vector4 original, original_over, original_down, original_text;
        get_watch_colors(theme, state->original_rgb_color, &original, &original_over, &original_down, &original_text);

        Vector4 current, current_over, current_down, current_text;
        get_watch_colors(theme, state->current_rgb_color, &current, &current_over, &current_down, &current_text);

        auto revert_theme = theme->apply_and_revert_button_theme;
        revert_theme.surface_color          = original;
        revert_theme.surface_color_over     = original_over;
        revert_theme.surface_color_down     = original_down;
        revert_theme.label_theme.text_color = original_text;

        auto apply_theme = theme->apply_and_revert_button_theme;
        apply_theme.surface_color          = current;
        apply_theme.surface_color_over     = current_over;
        apply_theme.surface_color_down     = current_down;
        apply_theme.label_theme.text_color = current_text;

        auto revert_area = get_rect(x0a, swatch_y0, bottom_button_width, button_height);
        auto sub_hash    = ui_get_hash(Source_Location::current(), 0);
        auto revert_hash = combine_hashes(hash, sub_hash);
        auto [revert_pressed, _revert_state] = button(revert_area, theme->revert_string, &revert_theme, NULL, revert_hash, loc);
        if (revert_pressed)
        {
            set_current_color_rgb(state, theme, state->original_rgb_color);
        }

        sub_hash        = ui_get_hash(Source_Location::current(), 0);
        auto apply_hash = combine_hashes(hash, sub_hash);
        auto apply_area = revert_area;
        apply_area.x += revert_area.w * 1.1f; // @Theme
        auto [apply_pressed, _apply_state] = button(apply_area, theme->apply_string, &apply_theme, NULL, apply_hash, loc);
        if (apply_pressed)
        {
            state->original_rgb_color   = state->current_rgb_color;
            *input_and_output_color_rgb = state->current_rgb_color;

            applied = true;
        }

        auto [stash_area, _remainder] = cut_top(stashed_color_area, button_height);
        stash_area.y     = apply_area.y;
        auto stash_theme = &theme->stash_button_theme;
        sub_hash         = ui_get_hash(Source_Location::current(), 0);
        auto stash_hash  = combine_hashes(hash, sub_hash);

        auto [stash_pressed, stash_state] = button(stash_area, String(""), stash_theme, NULL, stash_hash, loc);
        if (stash_pressed)
        {
            stash_color(state, state->current_rgb_color);
        }

        {
            auto triangle_margin = stash_area.w * .25f;
            Vector2 p0, p1, p2, p3;
            get_quad(stash_area, &p0, &p1, &p2, &p3);

            auto p4 = lerp(p2, p3, .5f);
            p4.y -= triangle_margin;
            auto p5 = p0 + Vector2(triangle_margin, triangle_margin);
            auto p6 = p1 + Vector2(-triangle_margin, triangle_margin);

            draw_arrow(stash_state, stash_theme, p4, p5, p6);
        }
    }

    auto sa = stashed_color_area;
    {
        //
        // Draw stashed colors.
        //
        auto stash_y1 = sa.y + sa.h;
        auto stash_y0 = circle_area.y;

        auto dy = stash_y1 - stash_y0;
        auto button_occupancy_ratio = .9f;
        auto margin = floorf(.5f + dy*(1 - button_occupancy_ratio)/static_cast<f32>(MAX_STASHED_COLORS - 1));
        auto button_size_y = floorf(.5f + (dy*button_occupancy_ratio)/MAX_STASHED_COLORS);

        auto region_x0 = sa.x;
        auto region_width = sa.w;

        auto button_width = sa.w;
        auto button_x0 = region_x0 + .5f*(region_width - button_width);
        auto button_x1 = region_x0 + button_width;

        rendering_2d_right_handed();
        set_shader(shader_argb_no_texture);
        immediate_begin();

        auto top_y = stash_y1;
        auto alpha = 1.0f;

        for (auto it : state->stashed_colors)
        {
            auto x0 = button_x0;
            auto y0 = top_y - button_size_y;
            auto x1 = button_x1;
            auto y1 = top_y;

            auto can_see = true; // @Incomplete:

            auto color_rect   = get_rect(x0, y0, x1-x0, y1-y0);
            auto color_status = get_status_flags(color_rect);

            if (can_see && (color_status & Status_Flags::OVER))
            {
                auto m = margin;
                auto backing_rect = expand(color_rect, margin);

                auto backing_color = Vector4(.5, .1, .1, 1); // @Incomplete: @Theme
                rounded_rectangle(backing_rect, theme->stashed_colors_shape, backing_color);

                if (mouse_button_left_state & KSTATE_START)
                {
                    set_current_color_rgb(state, theme, it, true, true);
                }
            }

            auto c = Vector4(it.x, it.y, it.z, alpha);
            rounded_rectangle(color_rect, theme->stashed_colors_shape, c);

            top_y -= button_size_y + margin;
        }
    }

    // auto vpad = button_height * .7f;
    // auto [_junk, mode_switch_area] = cut_top(controls_lower, vpad);
    // draw_mode_switch_buttons(state, theme, hash, loc, mode_switch_area);

    immediate_flush();

    return applied;
}

bool /*changed*/ color_slider(Rect r, Color_Picker_Theme *theme, Color_Picker_State *state, Slider_Index index, Vector3 hsl_color, f32 *value, bool *dragging, f32 dragging_t, f32 alpha)
{
    set_shader(shader_argb_no_texture);

    constexpr i32 NUM_SEGMENTS = 20;
    Vector2 last_p0, last_p1;
    Vector4 last_color;

    auto h = hsl_color.x;
    auto s = hsl_color.y;
    auto v = hsl_color.z;

    auto status = get_status_flags(r);
    if ((status & Status_Flags::OVER) && (mouse_button_left_state & KSTATE_START))
    {
        *dragging = true;
    }

    bool changed = false;
    Vector2 left_arrow_tip;
    Vector2 right_arrow_tip;
    Vector4 arrow_color;
    if (r.h)
    {
        //
        // Drawing the side arrows.
        //
        auto drag_y = mouse_y_float;

        auto t = (drag_y - r.y)/ r.h;
        Clamp(&t, 0.0f, 1.0f);

        auto pip_t = *value;
        auto dx = r.w * .1f;
        auto dy = dx;
        auto k = .5f;

        k   = lerp(k, 1, dragging_t);
        dx *= lerp(1, 2, dragging_t);
        dy *= lerp(1, 2, dragging_t);

        if (*dragging)
        {
            pip_t = t;

            // Side effects:
            *value = t;
            changed = true;
        }

        auto pa = Vector2(r.x,       r.y + pip_t*r.h);
        auto pb = Vector2(r.x + r.w, r.y + pip_t*r.h);

        arrow_color = Vector4(k, k, k, 1);

        auto p0 = pa;
        auto p1 = p0 + Vector2(-dx, dy*.5);
        auto p2 = p1 + Vector2(0, -dy);
        left_arrow_tip = p0;

        auto p3 = pb;
        auto p4 = p3 + Vector2(dx, dy*.5f);
        auto p5 = p4 + Vector2(0, -dy);
        right_arrow_tip = p3;

        rendering_2d_right_handed();
        set_shader(shader_argb_no_texture);
        immediate_begin();

        immediate_triangle(p0, p1, p2, argb_color(arrow_color));
        immediate_triangle(p3, p4, p5, argb_color(arrow_color));

        immediate_flush();
    }

    rendering_2d_right_handed();
    set_shader(shader_argb_no_texture);
    immediate_begin();
    for (i64 i = 0; i < NUM_SEGMENTS; ++i)
    {
        auto t = i / static_cast<f32>(NUM_SEGMENTS - 1);

        auto p0 = Vector2(r.x, r.y + t*r.h);
        auto p1 = Vector2(r.x + r.w, r.y + t*r.h);

        if (index == Slider_Index::Hue_Slider)
        {
            h = lerp(theme->slider_hue_min, theme->slider_hue_max, t);
            s = 1.0f;
            v = .5f;
        }
        else if (index == Slider_Index::Hue_Slider_Zoomed)
        {
            // @Cutnpaste
            auto dhue = theme->hue_range_in_zoomed_slider * .5f;
            auto low  = state->zoom_hue_center - dhue;
            auto high = state->zoom_hue_center + dhue;

            h = lerp(low, high, t);
            s = 1.0f;
            v = .5f;
        }
        else if (index == Slider_Index::Saturation_Slider)
        {
            s = t;
            v = .5f;
        }
        else if (index == Slider_Index::Value_Slider)
        {
            v = t;
        }

        auto rgb = hsl_to_rgb(Vector3(h, s, v));
        auto color = Vector4(rgb.x, rgb.y, rgb.z, alpha);

        if (i != 0)
        {
            auto last = argb_color(last_color);
            auto c = argb_color(color);
            immediate_triangle(last_p0, last_p1, p1, last, last, c);
            immediate_triangle(last_p0, p1, p0, last, c, c);
        }

        last_p0    = p0;
        last_p1    = p1;
        last_color = color;
    }

    if (index == Slider_Index::Hue_Slider)
    {
        //
        // Draw the calipers..
        //

        auto hue_range = (theme->slider_hue_max - theme->slider_hue_min);
        if (hue_range >= 0)
        {
            auto caliper_range     = theme->hue_range_in_zoomed_slider;
            auto caliper_distance  = (caliper_range / hue_range) * r.h * .5f;
            auto caliper_thickness = floorf(.5f + r.w * .025f); // @Theme:

            auto yy = (state->zoom_hue_center - theme->slider_hue_min)/hue_range * r.h + r.y;
            auto y_top    = yy + caliper_distance;
            auto y_bottom = yy - caliper_distance;

            auto alpha_top    = 1.0f;
            auto alpha_bottom = 1.0f;

            auto fade_margin = caliper_thickness*14; // @Theme:
            auto fade_denom = fade_margin;
            if (!fade_denom) fade_denom = 1;

            auto fade_zone_top    = r.y + r.h - fade_margin;
            auto fade_zone_bottom = r.y + fade_margin;

            auto excess_top = y_top - fade_zone_top;
            if (excess_top >= 0)
            {
                alpha_top = 1 - (excess_top / fade_denom);
                Clamp(&alpha_top, 0.0f, 1.0f);
            }

            auto excess_bottom = fade_zone_bottom - y_bottom;
            if (excess_bottom >= 0)
            {
                alpha_bottom = 1 - (excess_bottom / fade_denom);
                Clamp(&alpha_bottom, 0.0f, 1.0f);
            }

            auto q0 = left_arrow_tip;
            q0.y = y_top;
            auto q1 = right_arrow_tip;
            q1.y = y_top;

            auto q2 = q1 + Vector2(0, caliper_thickness);
            auto q3 = q0 + Vector2(0, caliper_thickness);

            auto ct = arrow_color; /* "color top" */
            ct.w = alpha_top;
            immediate_quad(q0, q1, q2, q3, argb_color(ct));

            auto r0 = left_arrow_tip;
            r0.y = y_bottom;
            auto r1 = right_arrow_tip;
            r1.y = y_bottom;

            auto r2 = r1 + Vector2(0, -caliper_thickness);
            auto r3 = r0 + Vector2(0, -caliper_thickness);

            auto cb = arrow_color; /* "color bottom" */
            cb.w = alpha_bottom;
            immediate_quad(r0, r1, r2, r3, argb_color(cb));
        }
    }

    immediate_flush();

    auto focus = has_focus(r);
    auto down  = focus && (mouse_button_left_state & KSTATE_DOWN);

    // Don't disable dragging until after we draw .. so we get at least one update if we do an intra-frame click.
    if (*dragging)
    {
        if (!down)
        {
            *dragging = false;
        }
    }

    return changed;
}

void draw_hsl_sliders(Rect area, Color_Picker_Theme *theme, Color_Picker_State *state)
{
    auto w = area.w;
    i32 sliders_count = static_cast<i32>(Slider_Index::Count);
    auto bar_width = w / (sliders_count*2 - 1);
    auto between_width = bar_width * .9f;
    auto left_margin = .5f * (w - ((sliders_count)*bar_width + (sliders_count - 1)*between_width));

    auto hsl_color = state->current_hsl_color;

    auto bar_x = area.x + left_margin;
    auto bar_y = area.y + w*.03f;
    auto bar_height = area.h - w*.03f;

    bar_x         = floorf(.5f + bar_x);
    bar_y         = floorf(.5f + bar_y);
    bar_width     = floorf(.5f + bar_width);
    bar_height    = floorf(.5f + bar_height);
    left_margin   = floorf(.5f + left_margin);
    between_width = floorf(.5f + between_width);

    auto alpha = 1.0f; // @Incomplete: Not handle active widgets (this is a global issue)...

    //
    // 'extra_wrapped_hue' is here because, when we have positioned the hue calipers so
    // that the zoomed area crosses past the primary hue bar (index 0)'s range,
    // we could use the zoom bar to move the primary hue cursor outside the range.
    // When this happens, rather than drawing the arrows out in space, we wrap
    // them down to the earlier repeat position. But we don't want to wrap the
    // actual current hue value.
    //
    auto hue_before_modifications = hsl_color.x;
    auto extra_wrapped_hue = hue_before_modifications;
    while (extra_wrapped_hue < theme->slider_hue_min) extra_wrapped_hue += 360.0f;
    while (extra_wrapped_hue > theme->slider_hue_max) extra_wrapped_hue -= 360.0f;

    auto hue_denom = theme->slider_hue_max - theme->slider_hue_min;
    if (!hue_denom) hue_denom = 1;
    auto h = (extra_wrapped_hue - theme->slider_hue_min) / hue_denom;

    auto r = get_rect(bar_x, bar_y, bar_width, bar_height);

    auto changed_unzoomed_hue = color_slider(r, theme, state, Slider_Index::Hue_Slider, hsl_color, &h, &state->dragging_in_h, state->dragging_in_h_t, alpha);
    auto changed_hsl = changed_unzoomed_hue;

    if (changed_unzoomed_hue)
    {
        hsl_color.x = lerp(theme->slider_hue_min, theme->slider_hue_max, h);
    }

    r.x += bar_width + between_width * .5f;
    auto dhue = theme->hue_range_in_zoomed_slider * .5f;
    auto hue_zoom_low   = state->zoom_hue_center - dhue;
    auto hue_zoom_high  = state->zoom_hue_center + dhue;
    auto hue_zoom_denom = (hue_zoom_high - hue_zoom_low);
    if (!hue_zoom_denom) hue_zoom_denom = 1;
    auto h_zoom = (hue_before_modifications - hue_zoom_low) / hue_zoom_denom;

    auto changed_zoomed_hue = color_slider(r, theme, state, Slider_Index::Hue_Slider_Zoomed, hsl_color, &h_zoom, &state->dragging_in_h_zoom, state->dragging_in_h_zoom_t, alpha);
    changed_hsl |= changed_zoomed_hue;
    if (changed_zoomed_hue)
    {
        hsl_color.x = lerp(hue_zoom_low, hue_zoom_high, h_zoom);
    }

    r.x += bar_width + between_width;
    changed_hsl |= color_slider(r, theme, state, Slider_Index::Saturation_Slider, hsl_color, &hsl_color.y, &state->dragging_in_s, state->dragging_in_s_t, alpha);

    r.x += bar_width + between_width;
    changed_hsl |= color_slider(r, theme, state, Slider_Index::Value_Slider, hsl_color, &hsl_color.z, &state->dragging_in_l, state->dragging_in_l_t, alpha);

    if (changed_hsl)
    {
        set_current_color_hsl(state, hsl_color, changed_unzoomed_hue);
        Vector3 rgb = hsl_to_rgb(hsl_color);

        set_current_color_rgb(state, theme, rgb, false, true);
    }
}

void draw_hsl_circle(Rect r, Color_Picker_Theme *theme, Color_Picker_State *state, f32 circle_r0, f32 circle_r1)
{
    constexpr i32 NUM_SEGMENTS = 100;

    // auto w  = circle_r1 * 2;
    auto r0 = circle_r0;
    auto r1 = circle_r1;
    auto center = Vector2(r.x + r.w*.5f, r.y + r.h*.5f);

    rendering_2d_right_handed();
    set_shader(shader_argb_no_texture);
    immediate_begin();

    {
        // In case the window resized, the rect changed, etc, refigure the ring point.
        state->ring_point = unit_vector(state->ring_point * (r0 + r1)) * .5f;
    }

    update_ring_point(state, center, r0, r1);

    Vector2 last_p0, last_p1;
    Vector4 last_color;

    auto alpha = 1.0f; // @Incomplete: We want a varied alpha if we do occlusion and stuff.

    for (i64 i = 0; i < NUM_SEGMENTS; ++i)
    {
        auto t = i / static_cast<f32>(NUM_SEGMENTS - 1);

        auto radians = TAU * t;
        auto degrees = 360.0f * t;
        auto dir = Vector2(cosf(radians), sinf(radians));

        auto p0 = center + dir*r0;
        auto p1 = center + dir*r1;

        auto h = degrees;
        auto s = 1.0f;
        auto v = .5f;

        auto rgb = hsl_to_rgb(Vector3(h, s, v));
        auto color = Vector4(rgb.x, rgb.y, rgb.z, alpha);

        if (i != 0)
        {
            immediate_triangle(last_p0, last_p1, p1, argb_color(last_color), argb_color(last_color), argb_color(color));
            immediate_triangle(last_p0, p1, p0, argb_color(last_color), argb_color(color), argb_color(color));
        }

        last_p0 = p0;
        last_p1 = p1;
        last_color = color;
    }

    // Draw the inner circle.
    auto disc_r = r0 * theme->inner_disc_radius;
    auto ir = 1.0f;
    if (disc_r) ir = 1.0/disc_r;

    update_disc_point(state, center, disc_r);

    auto h = (360 / TAU) * atan2f(state->ring_point.y, state->ring_point.x);
    if (h < 0) h += 360;

    auto value_vector      = unit_vector(Vector2(0,  1)); // @Incomplete: These values are meaningless right now!!!
    auto saturation_vector = unit_vector(Vector2(1, -1)); // @Incomplete: These values are meaningless right now!!!

    {
        // Draw a square now!
        auto r22 = sqrtf(2.0f) * .5f; // root2 / 2

        constexpr i32 STRIPS = 20;
        auto dtheta = 1 / static_cast<f32>(STRIPS);

        auto x_min = -r22 * disc_r; // Left.
        auto x_max = +r22 * disc_r; // Right.
        auto y_min = -r22 * disc_r; // Bottom.
        auto y_max = +r22 * disc_r; // Top.

        for (i32 j = 0; j < STRIPS; ++j)
        {
            auto y0 = lerp(y_min, y_max, j*dtheta);
            auto y1 = lerp(y_min, y_max, (j+1)*dtheta);

            for (i32 i = 0; i < STRIPS; ++i)
            {
                auto x0 = lerp(x_min, x_max, i*dtheta);
                auto x1 = lerp(x_min, x_max, (i+1)*dtheta);

                // s0..3 are circle-streched points, without the center added.
                auto s0 = circle_strech(x0, y0);
                auto s1 = circle_strech(x1, y0);
                auto s2 = circle_strech(x1, y1);
                auto s3 = circle_strech(x0, y1);

                // @Speed: Factor out ir.
                auto hsv0 = make_hsv(h, s0*ir, saturation_vector, value_vector);
                auto hsv1 = make_hsv(h, s1*ir, saturation_vector, value_vector);
                auto hsv2 = make_hsv(h, s2*ir, saturation_vector, value_vector);
                auto hsv3 = make_hsv(h, s3*ir, saturation_vector, value_vector);

                auto c0 = hsv_to_argb(hsv0);
                auto c1 = hsv_to_argb(hsv1);
                auto c2 = hsv_to_argb(hsv2);
                auto c3 = hsv_to_argb(hsv3);

                // q0..3 are the points we render. Just the circle points plus the center.
                auto q0 = s0 + center;
                auto q1 = s1 + center;
                auto q2 = s2 + center;
                auto q3 = s3 + center;

                immediate_quad(q0, q1, q2, q3, c0, c1, c2, c3);
            }
        }
    }

    //
    // Update colors.
    //
    {
        // @Bug: after we backsolve, this does not make the right hsv color.
        auto hsv = make_hsv(h, state->disc_point, saturation_vector, value_vector);
        auto rgb = hsv_to_rgb(hsv);

        set_current_color_rgb(state, theme, rgb, false);

        auto hsl = rgb_to_hsl(rgb);
        state->current_hsl_color = hsl;
    }

    //
    // Drawing the cursor for the ring point.
    //
    {
        auto dir = unit_vector(state->ring_point);

        f32 triangle_extension_distance;
        f32 apex_height;
        f32 triangle_border_width;
        auto border_color = Vector4(.05, .05, .05, .3);
        Vector4 triangle_color;
        {
            auto k = lerp(.05, 1., state->dragging_in_ring_t); // Used for foreground color.
            auto triangle_scale = lerp(.7, 1., state->dragging_in_ring_t);

            triangle_color = Vector4(k, k, k, 1); // @Theme

            apex_height = r1 * .12f * triangle_scale; // @Theme
            auto base_half_width = apex_height * .6f; // @Theme
            triangle_border_width = .35f * base_half_width; // @Theme
            triangle_extension_distance = apex_height * lerp(.2, .4, state->dragging_in_ring_t); // @Theme

            auto triangle_apex = center + (r1 + triangle_extension_distance - apex_height) * dir;
            draw_bordered_triangle(triangle_apex, apex_height, base_half_width, triangle_border_width, dir, triangle_color, border_color);
        }
        auto rectangle_retraction_distance = triangle_extension_distance * .38f; // @Theme
        auto rectangle_half_width = apex_height * .3f; // @Theme
        auto rectangle_height = apex_height * .35f; // @Theme
        auto rectangle_border_width = triangle_border_width * .6f; // @Theme
        auto rectangle_base = center + (r0 - rectangle_retraction_distance) * dir;
        auto rectangle_apex = rectangle_base + rectangle_height * dir;

        {
            auto strut = Vector2(-dir.y, dir.x);
            auto b = rectangle_half_width;
            auto p0 = rectangle_base - strut * b;
            auto p1 = rectangle_base + strut * b;
            auto p2 = p1 + dir * rectangle_height;
            auto p3 = p0 + dir * rectangle_height;

            auto bb = rectangle_border_width;
            auto bx = strut * bb;
            auto by = dir * bb;

            auto barycenter = (p0 + p2) * .5f;
            auto q0 = p0 - bx - by;
            auto q1 = p1 + bx - by;
            auto q2 = p2 + bx + by;
            auto q3 = p3 - bx + by;

            immediate_quad(q0, q1, q2, q3, border_color, border_color, border_color, border_color);
            immediate_quad(p0, p1, p2, p3, triangle_color, triangle_color, triangle_color, triangle_color);
        }
    }

    //
    // Draw the color marker for the disc point.
    //
    {
        auto triangle_scale = lerp(.24, .36, state->dragging_in_disc_t);
        auto k = lerp(.05, 1, state->dragging_in_disc_t);
        auto j = lerp(1, .05, state->dragging_in_disc_t);

        auto color = Vector4(k, k, k, 1); // @Theme
        auto border_color = Vector4(j, j, j, .3f); // @Theme

        auto w = r.w * .25f;
        auto pos = center + state->disc_point * disc_r;

        auto apex_height = r1 * .12f * triangle_scale; // @Theme
        auto base_half_width = apex_height * .6f; // @Theme
        auto triangle_distance_from_center = r1 * .13f * triangle_scale; // @Theme

        auto border_width = lerp(.7f, .45f, state->dragging_in_disc_t) * base_half_width;
        auto triangle_apex_N = pos + Vector2(0,  triangle_distance_from_center);
        auto triangle_apex_S = pos + Vector2(0, -triangle_distance_from_center);
        auto triangle_apex_E = pos + Vector2( triangle_distance_from_center, 0);
        auto triangle_apex_W = pos + Vector2(-triangle_distance_from_center, 0);

        draw_bordered_triangle(triangle_apex_N, apex_height, base_half_width, border_width, Vector2(0,  1), color, border_color);
        draw_bordered_triangle(triangle_apex_S, apex_height, base_half_width, border_width, Vector2(0, -1), color, border_color);
        draw_bordered_triangle(triangle_apex_E, apex_height, base_half_width, border_width, Vector2( 1, 0), color, border_color);
        draw_bordered_triangle(triangle_apex_W, apex_height, base_half_width, border_width, Vector2(-1, 0), color, border_color);
    }

    immediate_flush();
}

inline
void draw_bordered_triangle(Vector2 apex, f32 apex_height, f32 base_half_width, f32 triangle_border_width, Vector2 dir, Vector4 color, Vector4 border_color)
{
    auto strut = Vector2(-dir.y, dir.x);

    auto theta = atanf(apex_height / triangle_border_width); // Angle of a corner of the triangle.
    auto border_extension_distance = triangle_border_width / sinf(theta * .5f); // How far to go away from the barycenter to put the border around the triangle.

    auto triangle_base = apex + apex_height * dir;
    
    {
        auto p0 = triangle_base + strut * base_half_width;
        auto p1 = apex;
        auto p2 = triangle_base - strut * base_half_width;

        auto barycenter = (p0 + p1 + p2) * (1/3.0f);

        auto b0 = p0 + unit_vector(p0 - barycenter) * border_extension_distance;
        auto b1 = p1 + unit_vector(p1 - barycenter) * border_extension_distance;
        auto b2 = p2 + unit_vector(p2 - barycenter) * border_extension_distance;

        immediate_triangle(b0, b1, b2, argb_color(border_color));
        immediate_triangle(p0, p1, p2, argb_color(color));
    }
}

inline
bool colors_are_very_close(Vector3 a, Vector3 b)
{
    auto dist = glm::distance(a, b);
    return dist < .001f;
}

void stash_color(Color_Picker_State *state, Vector3 color)
{
    for (auto it : state->stashed_colors)
    {
        if (colors_are_very_close(it, color)) return;
    }

    if (state->stashed_colors.count >= MAX_STASHED_COLORS)
    {
        array_ordered_remove_by_index(&state->stashed_colors, 0);
    }

    array_add(&state->stashed_colors, color);
}

Vector3 rgb_to_hsv(Vector3 rgb);

void draw_hsl_readout(Rect area, Label_Theme *theme, Color_Picker_State *state, f32 line_height)
{
    auto hsl_color = state->current_hsl_color;

    //
    // HSL readout.
    //

    // We let h go negative, becase we want to remember where the cursor was
    // if it wraps off the botom or top ude to zoom, etc.. if we were to wrap hsl.x
    // when we handle the slider changes, it will pop the cursor. So we do it here.
    auto printed_h = hsl_color.x;
    while (printed_h < 0)   printed_h += 360;
    while (printed_h > 360) printed_h -= 360;

    auto text = tprint(String("H: %.2f;   S: %3d;   L: %3d"),
                       printed_h,
                       static_cast<i32>(.5f + hsl_color.y * 100),
                       static_cast<i32>(.5f + hsl_color.z * 100));

    auto [r, junk] = cut_top(area, line_height);

    label(r, text, theme);

    //
    // RGB readout.
    //
    auto rgb = state->current_rgb_color;
    text = tprint(String("R: %.2f;   G: %.2f;   B: %.2f"), rgb.x, rgb.y, rgb.z);

    r.y -= line_height;
    label(r, text, theme);

    r.y -= line_height;
    // @Incomplete:
}

void animate_value(f32 *value, bool condition, f32 up_rate, f32 down_rate, f32 dt)
{
    if (condition) *value = move_toward(*value, 1, dt * up_rate);
    else           *value = move_toward(*value, 0, dt * down_rate);
}

void update_animation_values(Color_Picker_State *state, f32 dt)
{
    constexpr f32 UP_RATE   = 9.0f;
    constexpr f32 DOWN_RATE = 3.0f;

    animate_value(&state->dragging_in_h_t, state->dragging_in_h, UP_RATE, DOWN_RATE, dt);
    animate_value(&state->dragging_in_s_t, state->dragging_in_s, UP_RATE, DOWN_RATE, dt);
    animate_value(&state->dragging_in_l_t, state->dragging_in_l, UP_RATE, DOWN_RATE, dt);
    animate_value(&state->dragging_in_h_zoom_t, state->dragging_in_h_zoom, UP_RATE, DOWN_RATE, dt);

    animate_value(&state->dragging_in_ring_t, state->dragging_in_ring, UP_RATE, DOWN_RATE, dt);
    animate_value(&state->dragging_in_disc_t, state->dragging_in_disc, UP_RATE, DOWN_RATE, dt);
}

f32 map(f32 p, f32 ss, f32 se, f32 ds, f32 de)
{
    return ds + (p - ss) * (de - ds) / (se - ss);
}

f32 shade(Vector2 pos, f32 dir)
{
    pos = rotate(pos, dir);
    auto h = cosf(pos.x);
    return map(pos.y, h, -h, 0.0f, 1.0f);
}

Vector3 make_hsv(f32 h, Vector2 p, Vector2 s_vector, Vector2 v_vector) // @Incomplete: Figure out what the fuck is going on with this function.
{
    // auto s_dot = glm::dot(s_vector, p);
    // auto v_dot = glm::dot(v_vector, p);

    // s_dot = (s_dot * .5f) + .5f;
    // v_dot = (v_dot * .5f) + .5f;

    // s_dot = powf(s_dot, 1.5f);

    // s_dot = std::clamp(s_dot, 0.0f, 1.0f);
    // v_dot = std::clamp(v_dot, 0.0f, 1.0f);

    auto p2 = p;
    p2.y = -p2.y;
    auto v_new = shade(p2,  0.0/3.0f * TAU); // ???
    auto s_new = shade(p2, -1.0/4.0f * TAU);

    Clamp(&s_new, 0.0f, 1.0f);
    s_new = powf(s_new, .85f);

    return Vector3(h, s_new, v_new);
}

Vector3 hsl_to_rgb(Vector3 hsl)
{
    auto h = hsl.x;
    auto s = hsl.y;
    auto l = hsl.z;

    while (h < 0)   h += 360.0f;
    while (h > 360) h -= 360.0f;

    f32 sat_r = 0, sat_g = 0, sat_b = 0;
    f32 tmp_r = 0, tmp_g = 0, tmp_b = 0;

    if (h < 120.0f)
    {
        sat_r = (120 - h) / 60.0f;
        sat_g = h / 60.0f;
        sat_b = 0;
    }
    else if (h < 240.0f)
    {
        sat_r = 0;
        sat_g = (240 - h) / 60.0f;
        sat_b = (h  - 120) / 60.0f;
    }
    else
    {
        sat_r = (h - 240) / 60.0f;
        sat_g = 0;
        sat_b = (360 - h) / 60.0f;
    }

    sat_r = std::min(sat_r, 1.0f);
    sat_g = std::min(sat_g, 1.0f);
    sat_b = std::min(sat_b, 1.0f);

    tmp_r = 2*s*sat_r + (1-s);
    tmp_g = 2*s*sat_g + (1-s);
    tmp_b = 2*s*sat_b + (1-s);

    f32 r, g, b;
    if (l < 0.5f)
    {
        r = l * tmp_r;
        g = l * tmp_g;
        b = l * tmp_b;
    }
    else
    {
        r = (1 - l) * tmp_r + 2*l - 1;
        g = (1 - l) * tmp_g + 2*l - 1;
        b = (1 - l) * tmp_b + 2*l - 1;
    }

    return Vector3(r, g, b);
}

inline
f32 fract(f32 x)
{
    return x - floorf(x);
}

Vector3 hsv_to_rgb(Vector3 hsv)
{
    auto cx = hsv.x;
    auto cy = hsv.y;
    auto cz = hsv.z;

    cx /= 360.0f;
    Vector3 p;
    p.x = fabs(fract(cx + 1)      * 6 - 3) - 1;
    p.y = fabs(fract(cx + 2/3.0f) * 6 - 3) - 1;
    p.z = fabs(fract(cx + 1/3.0f) * 6 - 3) - 1;

    Clamp(&p.x, 0.0f, 1.0f);
    Clamp(&p.y, 0.0f, 1.0f);
    Clamp(&p.z, 0.0f, 1.0f);

    p.x = lerp(1, p.x, cy);
    p.y = lerp(1, p.y, cy);
    p.z = lerp(1, p.z, cy);

    return Vector3(cz*p.x, cz*p.y, cz*p.z);
}

Vector4 hsv_to_argb(Vector3 hsv)
{
    // @Incomplete @Investigate:
    auto rgb   = hsv_to_rgb(hsv);
    auto alpha = 1.0f; // @Hardcoded

    return Vector4(rgb.x, rgb.y, rgb.z, alpha);
}

Vector2 circle_strech(f32 x, f32 y)
{
    auto len = get_inscribed_square_intersector_length(x, y);

    auto factor = 1.0f;
    if (len > .001f) factor = 1 / len; // We should never need this epsilon since the length does not get remotely that low.

    Vector2 result;
    result.x = x * factor;
    result.y = y * factor;

    return result;
}

f32 get_inscribed_square_intersector_length(f32 x, f32 y)
{
    // Because of 8-fold symmetry, just put us into the first octant and go.
    // There, we are intersecting with the line x == root2 / 2.
    x = fabs(x);
    y = fabs(y);

    if (y > x) swap_elements(&x, &y);
    if (x < .0001f) return 1; // Avoid the singularity.

    auto len = sqrtf(.5f + .5f * (y*y)/(x*x));
    return len;
}

void set_current_color_hsl(Color_Picker_State *state, Vector3 hsl, bool update_zoom_hue)
{
    state->current_hsl_color = hsl;

    auto theta = hsl.x * (TAU / 360.0f);
    state->ring_point = Vector2(cosf(theta), sinf(theta));

    if (update_zoom_hue)
    {
        state->zoom_hue_center = hsl.x;
    }        
}

Vector2 backsolve_disc_point_from_hsv_wrapped(Color_Picker_State *state, Vector3 hsv)
{
    constexpr auto EPSILON = 0.0001f;

    // Wrapped version.
    {
        auto s = hsv.y;
        auto v = hsv.z;

        auto x = s;
        auto y = v;

        if (x < .5f) x = -1;
        else x = 1;

        if (y < .5f) y = -1;
        else y = 1;

        auto len_total = sqrtf(x*x + y*y);
        if (len_total < EPSILON) return Vector2(0, 0);

        auto p = Vector2((s*2-1)/len_total, (v*2-1)/len_total);

        return p;
    }
}

Vector2 backsolve_disc_point_from_hsv(Color_Picker_State *state, Vector3 hsv)
{
    // @Incomplete: If we do wrapped disc, use the function above.

    // Some colors are not representable on the current disc!
    // To push them into disc space, we add an epsilon for now, which is kind of *ughhh*.
    // This doesn't affect the stored color, however (unless you click on the disc, which
    // would anyway only give you something that is inside the disc gamut!)

    constexpr f32 EPSILON = 0.0001f;

    f32 s = hsv.y;
    f32 v = hsv.z;

    if (s < EPSILON)
    {
        if (v == 0) return Vector2(0, -1);                // Bottom middle for black.
        if (v == 1) return unit_vector(Vector2(-1, +1));  // Upper left corner for white.

        s += EPSILON;
    }

    if (s > (1 - EPSILON))
    {
        if (v == 0) return Vector2(0, -1);                // Bottom middle for black.
        if (v == 1) return unit_vector(Vector2(+1, +1));  // Upper-right corner for saturated color.
        
        s -= EPSILON;
    }

    // if (v < EPSILON)       v = EPSILON;
    // if (v > (1 - EPSILON)) v = 1 - EPSILON;

    if (v < EPSILON)
    {
        v = EPSILON;
    }
    if (v > (1 - EPSILON))
    {
        v = 1 - EPSILON;
    }

    // This is becase s = sinf(tx * TAU * .25f);
    // So,            tx * TAU * .25f = asinf(s);
    // Therefore,     tx = asinf(s) * 4 / TAU;
    Clamp(&s, 0.0f, 1.0f);
    f32 tx = asinf(s) * (4.0f / TAU);

    Clamp(&v, 0.0f, 1.0f);
    f32 ty = asinf(v) * (4.0f / TAU);

    f32 alpha = 4 * (tx - .5f) * (tx - .5f);
    f32 beta  = 4 * (ty - .5f) * (ty - .5f);

    auto one_minus_ab = 1 - alpha*beta;
    if (one_minus_ab <= 0) // @Cleanup: Should have been coverred already, but leaving here for sanity check..
    {
        if (v < .5f) return Vector2(0, -1);               // Bottom middle for black.
        if (s < .5f) return unit_vector(Vector2(+1, +1)); // Upper left for white.

        return unit_vector(Vector2(+1, +1)); // Upper right for saturated color.
    }

    f32 ux_squared = (alpha - alpha*beta) / one_minus_ab;
    Clamp(&ux_squared, 0.0f, 1.0f);

    f32 ux = sqrtf(ux_squared);
    f32 uy = (ty - .5f) * 2 * sqrtf(1 - ux_squared);

    // @Hack: We lose the sign of ux in all this math, which means it can be simplified?
    // However, this needs more @Investigate and @Check.

    if (tx < .5f) ux = -ux;

    return Vector2(ux, uy);
}

Vector3 rgb_to_hsv(Vector3 rgb)
{
    f32 r = rgb.x;
    f32 g = rgb.y;
    f32 b = rgb.z;

    f32 rgb_min = std::min(r, std::min(g, b));
    f32 rgb_max = std::max(r, std::max(g, b));

    if (rgb_max == 0) return Vector3(0, 0, 0);

    f32 v = rgb_max;

    auto delta = rgb_max - rgb_min;

    auto s = delta / rgb_max;
    if (s == 0) // This covers also the case where rgb_max - rgb_min == 0 so it is fine for us to divide below.
    {
        return Vector3(0, 0, v);
    }

    f32 h;
    constexpr f32 K = (43 / 255.0f);

    if (rgb_max == r)
    {
        h = 0 + K * (g - b) / delta;
    }
    else if (rgb_max == g)
    {
        h = (85/255.0f) + K * (b - r) / delta;
    }
    else
    {
        h = (171/255.0f) + K * (r - g) / delta;
    }

    return Vector3(h * 360.0f, s, v);
}

Vector2 backsolve_disc_point_from_rgb(Color_Picker_State *state, Vector3 rgb)
{
    // We need to put the disc point back into the right place for the given rgb color!

    // First convert it to hsv, because hsv shares properties with a circle.
    auto hsv = rgb_to_hsv(rgb);

    printf("rgb (%f %f %f) == hsv (%f %f %f)\n", rgb.x, rgb.y, rgb.z, hsv.x, hsv.y, hsv.z);
    auto point = backsolve_disc_point_from_hsv(state, hsv);

    return point;
}

void set_current_color_rgb(Color_Picker_State *state, Color_Picker_Theme *theme /* We should consider packing this inside the state */, Vector3 rgb, bool update_hsl, bool update_disc_point)
{
    Vector3 hsl = rgb_to_hsl(rgb);

    // Wrap hsl.x into the range desired by theme.
    while (hsl.x < theme->slider_hue_min) hsl.x += 360;
    // while (hsl.x > theme->slider_hue_max) hsl.x -= 360;

    // Prefer the lower value on the slider, if there's more than 360 degrees!!!
    // The hue_max check above is redundant with this so I just commented it out?!?!
    while ((hsl.x - theme->slider_hue_min) > 360) hsl.x -= 360;

    if (update_hsl) set_current_color_hsl(state, hsl);

    if (update_disc_point)
    {
        // We provide an option for setting whether or not to update_disc_point, so that
        // when the user is actually moving the disc point around, we let them go wherever
        // they want around the circumfrence; we don't want to introduce numerical errror
        // by recomputing it, and we also don't want the disc point to snap to the edge
        // of the arcs (to fix singularities) while the user is freely moving.
        state->disc_point = backsolve_disc_point_from_rgb(state, rgb);
    }

    state->current_rgb_color = rgb;
}

Vector3 rgb_to_hsl(Vector3 rgb)
{
    f64 the_min = std::min(rgb.x, std::min(rgb.y, rgb.z));
    f64 the_max = std::max(rgb.x, std::max(rgb.y, rgb.z));

    auto delta = the_max - the_min;
    auto l = (the_max + the_min) / 2.0; // Lightness.

    f64 s = 0;
    if ((0 < l) && (l < 1))
    {
        if (l < .5f)
        {
            s = delta / (2*l);
        }
        else
        {
            s = delta / (2 - 2*l);
        }
    }

    f64 h = 0;
    if (delta > 0)
    {
        if ((the_max == rgb.x) && (the_max != rgb.y))
        {
            h += (rgb.y - rgb.z)/delta;
        }

        if ((the_max == rgb.y) && (the_max != rgb.z))
        {
            h += 2 + (rgb.z - rgb.x)/delta;
        }

        if ((the_max == rgb.z) && (the_max != rgb.x))
        {
            h += 4 + (rgb.x - rgb.y)/delta;
        }

        h *= 60;
    }

    // Is this better than using fmod?
    while (h < 0)   h += 360;
    while (h > 360) h -= 360;

    return Vector3(static_cast<f32>(h), static_cast<f32>(s), static_cast<f32>(l));
}

void update_ring_point(Color_Picker_State *state, Vector2 center, f32 r0, f32 r1)
{
    auto pos   = Vector2(mouse_x_float, mouse_y_float);
    auto delta = pos - center;

    auto len_orig = glm::length(delta);
    auto len = len_orig;
    Clamp(&len, r0, r1);
    auto inside = (r0 <= len_orig) && (len_orig <= r1);

    if (inside && !state->dragging_in_ring)
    {
        if (mouse_button_left_state & KSTATE_START)
        {
            state->dragging_in_ring = true;
        }
    }

    if (state->dragging_in_ring)
    {
        state->dragging_in_ring = !(mouse_button_left_state & KSTATE_END);
        if (r1) state->ring_point = unit_vector(delta)*len / r1;
    }
}

void update_disc_point(Color_Picker_State *state, Vector2 center, f32 radius)
{
    auto pos   = Vector2(mouse_x_float, mouse_y_float);
    auto delta = pos - center;
    auto len_orig = glm::length(delta);
    auto len = len_orig;

    Clamp(&len, 0.0f, radius);
    auto inside = len_orig <= radius;

    if (inside && !state->dragging_in_disc)
    {
        if (mouse_button_left_state & KSTATE_START)
        {
            state->dragging_in_disc = true;
        }
    }

    if (state->dragging_in_disc)
    {
        state->dragging_in_disc = !(mouse_button_left_state & KSTATE_END);
        if (radius) state->disc_point = unit_vector(delta)*len / radius;
    }
}

Rect expand(Rect r, f32 amount)
{
    Rect result = r;
    result.x -= amount;
    result.y -= amount;
    result.w += amount*2;
    result.h += amount*2;

    return result;
}  

/*
void draw_mode_switch_buttons(Rect area, Color_Picker_State *state, Color_Picker_Theme *theme, u64 hash, Source_Location loc)
{
    constexpr auto NUM_BUTTONS = 4;
    auto button_width  = area.w / (NUM_BUTTONS + 1);
    auto button_height = button_width;

    auto side_margin = (area.w - NUM_BUTTONS * button_width) * .5f;

    auto [buttons_strip, junk] = cut_top(area, button_height);
    auto [junk2, buttons_area] = cut_left(buttons_strip, side_margin);

    Rect r;
    auto remainder = buttons_area;
    for (i32 i = 0; i < NUM_BUTTONS; ++i)
    {
        auto [_r, _remainder] = cut_left(remainder, button_width);
        r = _r;
        remainder = _remainder;

        auto sub_hash = get_hash(Source_Location::current(), i);
        auto button_hash = combine_hashes(hash, sub_hash);

        Texture_Map *map;
        if (it == 0) map = map_mode_circle;
        else if (it == 1) map = map_mode_hsv;
        else if (it == 2) map = map_mode_rgb;
        else if (it == 3) map = map_mode_numbers;

        auto [pressed, button_state] = button(r, String(""), theme->mode_switch_button_theme, button_hash, loc, map);

        if (pressed)
        {
            stop_grabbing_text_input(state);
            state->color_pick_mode = it;
        }
    }
}
*/
