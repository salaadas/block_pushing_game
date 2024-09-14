#include "ui.h"

// Todo: :IncompleteUI
//
// * Do subwindows.
// * Do popups.
// * Do active_widgets.
// * Do occlusion.

// * Bug in the color picker that does not revert the disc point back to the original.
// * Cleanup all the active widgets stuff.
//
// * A bug that happens when you switch between the normal text input and the text input of the slider, it will start dragging your cursor even though you just clicked in.
// BUG: ring point and disc point does not update when you choose stashed color.
// * Cleanup color_picker.cpp
// 
// * Get rid of the need for begins_with_count?

#include "time_info.h"
#include "main.h"
#include "opengl.h"
#include "draw.h"
#include "table.h"
#include "events.h"
#include "file_utils.h"

#include "popups.h"
#include "ui_occlusion.h"
#include "color_picker.h"
#include "text_input.h"
#include "subwindow.h"

RArr<Rect> scissor_stack;

Overall_Theme default_overall_theme;

f32 ui_current_dt = 0.0f;

// i32 mouse_z = 0;
i32 mouse_x = 0;
i32 mouse_y = 0;
f32 mouse_x_float = 0;
f32 mouse_y_float = 0;

Key_Current_State mouse_button_left_state;
Key_Current_State mouse_button_right_state;

Dynamic_Font *default_editor_font = NULL;

// Load these, or do something better.
Texture_Map *map_radiobox_full  = NULL;
Texture_Map *map_radiobox_empty = NULL;
Texture_Map *map_checkbox_full  = NULL;
Texture_Map *map_checkbox_empty = NULL;

Text_Input text_input_a; // :DeprecateMe  for demo only
Text_Input text_input_b; // :DeprecateMe  for demo only
Text_Input text_input_c; // :DeprecateMe  for demo only
Text_Input text_input_d; // :DeprecateMe  for demo only

// :DeprecateMe Autocomplete function for text_input demo:
String tea_names[] = {
    String("Earl Gray"),
    String("Singapore Breakfast"),
    String("Bengal Spice"),
    String("Tie Guyanin"),
    String("Lapsang Souching"),
    String("Blue Mountain Pu-Erh"),
    String("Blue dude"),
    String("Juicer"),
    String("Motherfreaker"),
    String("Snore and Peace"),
    String("Genmaicha"),
    String("Jasmine Pearls"),
    String("Sencha"),
    String("Gyokuro"),
    String("White Claw"),
};
i64 tea_names_count = 14; // Temporary hack for tea_names :(

// For slider demo.
f32 slider_floating_point_value = 10.0f;
i32 slider_integral_value = 3;

// For label demo.
Dynamic_Font *cursive_font = NULL;

// For scrollable region demo.
f32 demo_scroll_value = 0.0f;

// For dropdown demo.
RArr<String> awesome_names;
i32 current_name_choice = 0;

// For subwindow demo.
Rect my_subwindow_a;
Rect my_subwindow_b;

//
// :CommonInput
// Probably @Temporary: General purposed text input for widgets that has a text input field.
// This is temporary because using a common text input means that we will be dumping
// any temporary string as we switch widgets back and forth. In the long run, we
// maybe want to instantiate a separate Text_Input for each widget that can have one.
//
Text_Input common_input_for_general_widget_use;
void *common_input_being_used_by = NULL; // A pointer to the state record that is using 'common_input_for_general_widget_use'

Text_Input *grab_the_common_text_input(void *used_by)
{
    if (common_input_being_used_by != used_by)
    {
        common_input_being_used_by = used_by;
        common_input_for_general_widget_use.entered = false;
    }

    if (!common_input_for_general_widget_use.initted)
    {
        init(&common_input_for_general_widget_use);
    }

    return &common_input_for_general_widget_use;
}

void stop_grabbing_the_common_text_input(void *used_by)
{
    assert((used_by != NULL));

    if (common_input_being_used_by == used_by)
    {
        common_input_being_used_by = NULL;
        common_input_for_general_widget_use.entered = false;
        // reset(&common_input_for_general_widget_use); // @Hack: We shouldn't just blatantly reset the inputting text when you stop using it, or should we? @Bug @Bug @Bug
    }
}

Text_Input *am_i_grabbing_the_common_input(void *used_by)
{
    if (common_input_being_used_by == used_by)
    {
        return &common_input_for_general_widget_use;
    }

    return NULL;
}

// @Cleanup: Get rid of the need for begins_with_count?
i64 begins_with_count(String s, String prefix)
{
    for (i64 i = 0; i < prefix.count; ++i)
    {
        if ((i >= s.count) || (s[i] != prefix[i])) return i;
    }

    return prefix.count;
}

i32 auto_complete_teas(String s, void *data, RArr<String> *match_results)
{
    i64 longest_match_length = 0;
    for (i64 i = 0; i < tea_names_count; ++i)
    {
        auto tea_name = tea_names[i];

        auto matched_count = begins_with_count(tea_name, s);
        if (matched_count == s.count)
        {
            array_add(match_results, tea_name);
            longest_match_length = std::max(s.count, longest_match_length);
        }

        longest_match_length = std::max(matched_count, longest_match_length);
    }

    return longest_match_length;
}

inline
bool is_visible_in_scissor(Rect r)
{
    // @Incomplete: Do this when we have a notion of global focus. :LayoutUI
    return true;
}

bool is_inside(f32 x, f32 y, Rect r) // Check if point is inside rect.
{
    return (x >= r.x) && (x <= r.x + r.w) && (y >= r.y) && (y <= r.y + r.h);
}

inline
bool is_visible_in_scissor(f32 x, f32 y)
{
    if (!scissor_stack) return true;

    auto top = &scissor_stack[0];
    return is_inside(x, y, *top);
}

Rect get_rect(f32 x, f32 y, f32 w, f32 h)
{
    Rect r;
    r.x = x;
    r.y = y;
    if (w < 0) r.x += w;
    if (h < 0) r.y += h;

    r.w = fabs(w);
    r.h = fabs(h);

    return r;
}

inline
void set_scissor(Rect r)
{
    //
    // We are adding 1 to the width and height to the scissoring region
    // because if we don't it eats into the frame thickness.
    // *However*, this is wrong for cases when up pass in a zero-width or zero-height
    // rect.
    // So for now, I'm keeping things this way.
    //

    auto x = static_cast<i32>(r.x);
    auto y = static_cast<i32>(r.y);
    auto w = static_cast<i32>(r.w) + 1;
    auto h = static_cast<i32>(r.h) + 1;

    glEnable(GL_SCISSOR_TEST);
    glScissor(x, y, w, h);
}

inline
void clear_scissor()
{
    glDisable(GL_SCISSOR_TEST);
}

void push_scissor(Rect r)
{
    if (scissor_stack)
    {
        auto top = &scissor_stack[scissor_stack.count - 1];

        // Intersect rectangles.
        auto x = std::max(top->x, r.x);
        auto y = std::max(top->y, r.y);
        auto w = std::max(std::min(top->x + top->w, r.x + r.w) - x, 0.0f);
        auto h = std::max(std::min(top->y + top->h, r.y + r.h) - y, 0.0f);

        r = get_rect(x, y, w, h);
    }

    set_scissor(r);
    array_add(&scissor_stack, r);
}

void pop_scissor()
{
    pop(&scissor_stack);

    if (scissor_stack)
    {
        auto r = scissor_stack[scissor_stack.count - 1];
        set_scissor(r);
    }
    else
    {
        clear_scissor();
    }
}

f32 em(Dynamic_Font *font, f32 x)
{
    return font->em_width * x;
}

void get_quad(Rect r, Vector2 *p0, Vector2 *p1, Vector2 *p2, Vector2 *p3)
{
    p0->x = r.x;
    p1->x = r.x + r.w;
    p2->x = r.x + r.w;
    p3->x = r.x;

    p0->y = r.y;
    p1->y = r.y;
    p2->y = r.y + r.h;
    p3->y = r.y + r.h;
}

// inline
Vector4 darken(Vector4 color, f32 t)
{
    Vector4 result;

    result.x = lerp(color.x, 0, t);
    result.y = lerp(color.y, 0, t);
    result.z = lerp(color.z, 0, t);
    result.w = lerp(color.w, 0, t);

    return result;
}

// inline
Vector4 whiten(Vector4 color, f32 t)
{
    Vector4 result;

    result.x = lerp(color.x, 1, t);
    result.y = lerp(color.y, 1, t);
    result.z = lerp(color.z, 1, t);
    result.w = lerp(color.w, 1, t);

    return result;
}

my_pair<Rect /*right*/, Rect /*remainder*/> cut_right(Rect r, f32 amount)
{
    auto remainder = r;
    remainder.w -= amount;

    auto right = r;
    right.x += remainder.w;
    right.w = amount;

    return {right, remainder};
}

my_pair<Rect /*left*/, Rect /*remainder*/> cut_left(Rect r, f32 amount)
{
    auto left = r;
    left.w = amount;

    auto remainder = r;
    remainder.x += amount;
    remainder.w -= amount;

    return {left, remainder};
}

my_pair<Rect /*top*/, Rect /*remainder*/> cut_top(Rect r, f32 amount)
{
    auto remainder = r;
    remainder.h -= amount;

    auto top = r;
    top.y += remainder.h;
    top.h = amount;

    return {top, remainder};
}

my_pair<Rect /*bottom*/, Rect /*remainder*/> cut_bottom(Rect r, f32 amount)
{
    auto remainder = r;
    remainder.h -= amount;
    remainder.y += amount;

    auto bottom = r;
    bottom.h = amount;

    return {bottom, remainder};
}

inline
f32 vertically_centered_text_baseline(Dynamic_Font *font, f32 rect_height)
{
    auto active_height = font->character_height * 0.55f;
    auto pad = rect_height - active_height;

    return floorf(pad * 0.5);
}

inline
my_pair<f32, f32> text_origin(Rect r, Label_Theme *theme, f32 text_width)
{
    auto x = r.x;
    auto font = theme->font;

    switch (theme->alignment)
    {
        case Text_Alignment::Left:   x += em(font, theme->alignment_pad_ems); break;
        case Text_Alignment::Center: x += (r.w - text_width) * 0.5f; break;
        case Text_Alignment::Right:  x += (r.w - text_width) - em(font, theme->alignment_pad_ems); break;
    }

    auto vertical_pad = theme->text_baseline_vertical_position;
    if (vertical_pad == FLT_MAX)
    {
        vertical_pad = vertically_centered_text_baseline(font, r.h);
    }
    else
    {
        vertical_pad *= r.h;
    }

    auto y = r.y + vertical_pad;

    // This is because we are drawing the font using textured atlasses, and the draw_text procedure accepts integer
    // coordinates. Maybe we will do something else when we switch to sub-pixel font drawing.
    x = floorf(x + .5f);
    y = floorf(y + .5f);
    return {x, y};
}

// Instead of falling back to the label theme inside the overall theme,
// we require you to instantiate a label theme on your own and passes it here.
// Reason being is that many, many, many widgets are dependent on the label
// and have already used the default overall theme, and there label theme is
// already defined well.
void label(Rect r, String text, Label_Theme *theme)
{
    auto text_width = get_text_width(theme->font, text);

    if (text_width > r.w) push_scissor(r);
    defer { if (text_width > r.w) pop_scissor(); };

    auto [text_x, text_y] = text_origin(r, theme, text_width);
    draw_text(theme->font, static_cast<i32>(text_x), static_cast<i32>(text_y), text, theme->text_color);
}

void update_over_state(Button_State *state, Button_Theme *theme, u32 status_flags, f32 dt)
{
    if (status_flags & Status_Flags::OVER)
    {
        state->over_effect_t = move_toward(state->over_effect_t, 1, dt * theme->over_fade_in_speed);
        state->over_duration += dt;
    }
    else
    {
        state->over_effect_t = move_toward(state->over_effect_t, 0, dt * theme->over_fade_out_speed);
        if (!state->over_effect_t) state->over_duration = 0;
    }

    if (status_flags & Status_Flags::DOWN)
    {
        state->down_effect_t = move_toward(state->down_effect_t, 1, dt * theme->down_fade_in_speed);
        state->down_duration += dt;
    }
    else
    {
        state->down_effect_t = move_toward(state->down_effect_t, 0, dt * theme->down_fade_out_speed);
        if (!state->down_effect_t) state->down_duration = 0;
    }
}

f32 update_action_durations(Button_State *state, Button_Theme *theme, bool begin, f32 dt)
{
    if (begin)
    {
        state->action_duration   = 0;
        state->action_duration_2 = 0;
    }

    auto pressed_factor = 0.0f;
    if (state->action_duration >= 0) // Some juice when you push the button.
    {
        state->action_duration += dt;
        auto denom = theme->press_duration;
        if (!denom) denom = 1;

        auto factor = state->action_duration / denom;
        pressed_factor = 1 - factor;
        if (pressed_factor < 0) state->action_duration = -1;

        pressed_factor = std::clamp(pressed_factor, 0.f, 1.f);
        pressed_factor *= pressed_factor;
    }

    if (state->action_duration_2 >= 0) // Some juice when you push the button.
    {
        state->action_duration_2 += dt;
    }

    return pressed_factor;
}

my_pair<f32 /*over_factor*/, f32 /*pressed_flash_factor*/> update_production_value_button(Rect r, bool changed, Button_State *state, u32 status_flags, Button_Theme *theme)
{
    assert(theme);

    auto dt = ui_current_dt;
    update_over_state(state, theme, status_flags, dt);

    auto pressed_factor = update_action_durations(state, theme, changed, dt);

    auto blend_factor = sinf(TAU * state->over_duration * .5f);
    blend_factor += 1;
    blend_factor *= .5f;
    blend_factor = std::clamp(blend_factor, 0.0f, 1.0f);

    constexpr auto STROBE_BASE   = .6f;
    constexpr auto STROBE_HEIGHT = .4f;

    auto base = STROBE_BASE * state->over_effect_t;
    auto over_factor = base + state->over_effect_t * blend_factor * .4f;

    return {over_factor, pressed_factor};
}

void core_draw_button(Rect r, String text, u32 status_flags, Button_State *state, Button_Theme *theme, Texture_Map *map)
{
    if (!is_visible_in_scissor(r)) return;
    // if (!theme) theme = &default_overall_theme.button_theme;

    bool changed = status_flags & Status_Flags::PRESSED;
    auto [over_factor, flash_factor] = update_production_value_button(r, changed, state, status_flags, theme);

    auto surface_color = get_color_for_button(theme, state, over_factor, flash_factor);

    if (!map)
    {
        set_shader(shader_argb_no_texture);
    }
    else
    {
        set_shader(shader_argb_and_texture);
        set_texture(String("diffuse_texture"), map);
    }

    immediate_begin();
    rounded_rectangle(r, theme->rectangle_shape, surface_color);

    // Draw the label.
    if (text)
    {
        auto color = theme->label_theme.text_color;
        color = lerp(color, theme->text_color_over,    over_factor);
        color = lerp(color, theme->text_color_pressed, flash_factor);

        Label_Theme label_theme = theme->label_theme;
        label_theme.text_color  = color;

        auto offset_t = state->down_effect_t * state->down_effect_t; // @Note: Bias towards less offset, to make it feels like the text springs back faster.
        auto offset = theme->text_offset_when_down * offset_t;
        r.x += em(label_theme.font, offset.x);
        r.y += em(label_theme.font, offset.y);

        label(r, text, &label_theme);
    }

    immediate_flush();
}

bool ui_can_see_event(Key_Code key_code, void *handle, GLFWwindow *glfw_window, i32 x, i32 y)
{
    return true;

    // if (key == CODE_UNKNOWN) return false;

    // if (ui_disabled) return false;

    // if (!handle) return true;
    // if (ui_grabbed_by)
    // {
    //     if (ui_grabbed_by != handle)  return false;
    //     return true;
    // }

    // auto is_mouse_event = (key == CODE_MOUSE_LEFT) || (key == CODE_MOUSE_RIGHT);

    // if (is_typing() && !is_mouse_event)
    // {
    //     return false;
    // }

    // @Incomplete:
}

Key_Current_State ui_get_button_state(Key_Code key_code, void *handle = NULL)
{
    auto state = input_button_states[key_code];

    if (state != KSTATE_NONE)
    {
        // Put this inside 'if state' for speed purposes.
        if (!ui_can_see_event(key_code, handle, glfw_window, mouse_x, mouse_y)) return KSTATE_NONE;
    }

    return state;
}

// Returns Status_Flags type
u32 get_status_flags(Rect r)
{
    u32 status = 0;

    auto x = mouse_x_float;
    auto y = mouse_y_float;

    if (!is_visible_in_scissor(x, y)) return status;

    if (is_inside(x, y, r))
    {
        status |= Status_Flags::OVER;

        if (mouse_button_left_state & KSTATE_START) status |= Status_Flags::PRESSED;
        // if (mouse_button_left_state & KSTATE_DOWN)  status |= Status_Flags::DOWN;
    }

    return status;
}

bool has_focus(Rect r)
{
    // @Incomplete: No notion of global focus yet.
    return true;
}

my_pair<bool /*pressed*/, Button_State*> button(Rect r, String text, Button_Theme *theme, Texture_Map *map, i64 identifier, Source_Location loc)
{
    if (!theme) theme = &default_overall_theme.button_theme;
    assert(theme->label_theme.font);

    auto hash   = ui_get_hash(loc, identifier);
    auto status = get_status_flags(r);
    auto state  = find_or_create_state(&table_button, hash);
    defer { stop_using_state(&state->widget); };

    state->released = false; // @Cleanup: This flag handling stuff need not be this complicated.

    auto result = false;
    if (state->pressed)
    {
        if (!(mouse_button_left_state & KSTATE_DOWN))
        {
            if (status & Status_Flags::OVER) state->released = true;
            state->pressed = false;
        }
    }
    else
    {
        if (status & Status_Flags::PRESSED)
        {
            state->pressed = true; // Start the pressing.
            result = true;
        }
    }

    if (state->pressed)
    {
        // @Cleanup: This just tells core_draw_button that we are pressed, which is kinda dumb.
        status |= Status_Flags::DOWN;
    }

    core_draw_button(r, text, status, state, theme, map);

    return {result, state};
}

void rounded_rectangle(Rect r, Rectangle_Shape shape, Vector4 color)
{
    f32 radius_pixels = get_float_parameter(r, shape.relativeness, shape.roundedness, shape.roundedness);
    f32 frame_pixels  = get_float_parameter(r, shape.relativeness, shape.frame_thickness, shape.frame_thickness);

    auto shortest = std::min(r.w, r.h);
    radius_pixels = std::clamp(radius_pixels, 0.0f, shortest * .5f);
    frame_pixels  = std::clamp(frame_pixels, 0.0f, shortest * .5f);

    if ((frame_pixels > 0) && (frame_pixels < 1)) frame_pixels = 1; // Give them at least 1 pixel if they are asking for a frame...

    radius_pixels = floorf(.5f + radius_pixels);
    frame_pixels  = floorf(.5f + frame_pixels);

    auto flags = shape.rounding_flags;

    if (radius_pixels == 0)
    {
        flags = 0; // Resetting the flags to 0 because we won't do the corners, but we still do the reset of the code, because we want to draw frames, etc and don't want to dupe that code here.
    }

    Vector2 outer0, outer1, outer2, outer3;
    get_quad(r, &outer0, &outer1, &outer2, &outer3);

    // These are the inner points offsetted inwards by an amount of 'radius_pixels'.
    auto inner0 = outer0 + Vector2(radius_pixels, radius_pixels);
    auto inner1 = inner0;  inner1.x += r.w - 2*radius_pixels;
    auto inner2 = inner1;  inner2.y += r.h - 2*radius_pixels;
    auto inner3 = inner0;  inner3.y = inner2.y;

    // These are the points that make up the rectangle strips around
    // the perimeter (not including the rounded edges).
    auto south0 = outer0;  south0.x = inner0.x;
    auto south1 = outer1;  south1.x = inner1.x;
    auto north0 = outer3;  north0.x = inner0.x;
    auto north1 = outer2;  north1.x = inner1.x; 
    auto east0  = outer1;  east0.y  = inner1.y;
    auto east1  = outer2;  east1.y  = inner2.y;
    auto west0  = outer0;  west0.y  = inner0.y;
    auto west1  = outer3;  west1.y  = inner3.y;

    auto color_argb = argb_color(color);

    // Drawing the inner rectangle as well as the four adjacent flaps first.
    immediate_quad(inner0, inner1, inner2, inner3, color_argb);

    immediate_quad(south0, south1, inner1, inner0, color_argb);
    immediate_quad(inner3, inner2, north1, north0, color_argb);
    immediate_quad(inner1, east0,  east1,  inner2, color_argb);
    immediate_quad(west0,  inner0, inner3, west1,  color_argb);

    //
    // Draw the frame.
    //
    frame_pixels = floorf(.5f + frame_pixels);
    auto frame_color = argb_color(whiten(color, .5f)); // @Hardcode @Theme

    auto frame_s0 = south0;  frame_s0.y += frame_pixels;
    auto frame_s1 = south1;  frame_s1.y += frame_pixels;
    auto frame_n0 = north0;  frame_n0.y -= frame_pixels;
    auto frame_n1 = north1;  frame_n1.y -= frame_pixels;
    auto frame_e0 = east0;   frame_e0.x -= frame_pixels;
    auto frame_e1 = east1;   frame_e1.x -= frame_pixels;
    auto frame_w0 = west0;   frame_w0.x += frame_pixels;
    auto frame_w1 = west1;   frame_w1.x += frame_pixels;

    if (frame_pixels)
    {
        immediate_quad(south0,   south1,   frame_s1, frame_s0, frame_color);
        immediate_quad(frame_e0, east0,    east1,    frame_e1, frame_color);
        immediate_quad(frame_n0, frame_n1, north1,   north0,   frame_color);
        immediate_quad(west0,    frame_w0, frame_w1, west1,    frame_color);
    }

    auto denom = shape.pixels_per_edge_segment;
    if (denom <= 0)
    {
        denom = 4.0f; // Fall back to some value. Idk if this is good...
    }

    auto num_segments = static_cast<i32>(floorf(.5f + TAU * radius_pixels/denom));
    if (num_segments < 1) num_segments = 1;

    if (!(flags & Rectangle_Rounding_Flags::NORTHEAST))
    {
        immediate_quad(inner2, east1, outer2, north1, color_argb);

        auto north_east_inner = Vector2(frame_e1.x, frame_n1.y);
        immediate_quad(frame_n1, north_east_inner, outer2, north1, frame_color);
        immediate_quad(frame_e1, east1, outer2, north_east_inner, frame_color);
    }
    if (!(flags & Rectangle_Rounding_Flags::NORTHWEST))
    {
        immediate_quad(west1,  inner3, north0, outer3, color_argb);

        auto north_west_inner = Vector2(frame_w1.x, frame_n1.y);
        immediate_quad(north_west_inner, frame_n0, north0, outer3, frame_color);
        immediate_quad(west1, frame_w1, north_west_inner, outer3, frame_color);
    }
    if (!(flags & Rectangle_Rounding_Flags::SOUTHEAST))
    {
        immediate_quad(south1, outer1, east0,  inner1, color_argb);

        auto south_east_inner = Vector2(frame_e0.x, frame_s1.y);
        immediate_quad(south1, outer1, south_east_inner, frame_s1, frame_color);
        immediate_quad(south_east_inner, outer1, east0, frame_e0, frame_color);
    }
    if (!(flags & Rectangle_Rounding_Flags::SOUTHWEST))
    {
        immediate_quad(outer0, south0, inner0, west0,  color_argb);

        auto south_west_inner = Vector2(frame_w0.x, frame_s0.y);
        immediate_quad(outer0, south0, frame_s0, south_west_inner, frame_color);
        immediate_quad(outer0, south_west_inner, frame_w0, west0, frame_color);
    }

    auto dtheta = TAU * .25f * (1 / static_cast<f32>(num_segments));
    f32 last_theta = 0;
    for (i32 i = 0; i < num_segments; ++i)
    {
        auto theta0 = last_theta;
        auto theta1 = theta0 + dtheta;
        last_theta  = theta1;

        auto ct0 = cosf(theta0);
        auto ct1 = cosf(theta1);
        auto st0 = sinf(theta0);
        auto st1 = sinf(theta1);

        // For the actual corners.
        auto w0 = Vector2(ct0 * radius_pixels, st0 * radius_pixels);
        auto w1 = Vector2(ct1 * radius_pixels, st1 * radius_pixels);

        // For the frames.
        auto fr0 = Vector2(ct0 * (radius_pixels - frame_pixels), st0 * (radius_pixels - frame_pixels));
        auto fr1 = Vector2(ct1 * (radius_pixels - frame_pixels), st1 * (radius_pixels - frame_pixels));

        if (flags & Rectangle_Rounding_Flags::NORTHEAST)
        {
            immediate_triangle(inner2, inner2 + w0, inner2 + w1, color_argb);
            if (frame_pixels) immediate_quad(inner2 + fr1, inner2 + fr0, inner2 + w0, inner2 + w1, frame_color);
        }

        if (flags & Rectangle_Rounding_Flags::SOUTHWEST)
        {
            immediate_triangle(inner0, inner0 - w0, inner0 - w1, color_argb);
            if (frame_pixels) immediate_quad(inner0 - fr1, inner0 - fr0, inner0 - w0, inner0 - w1, frame_color);
        }

        auto m0 = Vector2(-w0.y, w0.x);
        auto m1 = Vector2(-w1.y, w1.x);
        auto mfr0 = Vector2(-fr0.y, fr0.x);
        auto mfr1 = Vector2(-fr1.y, fr1.x);
        if (flags & Rectangle_Rounding_Flags::NORTHWEST)
        {
            immediate_triangle(inner3, inner3 + m0, inner3 + m1, color_argb);
            if (frame_pixels) immediate_quad(inner3 + mfr1, inner3 + mfr0, inner3 + m0, inner3 + m1, frame_color);
        }

        if (flags & Rectangle_Rounding_Flags::SOUTHEAST)
        {
            immediate_triangle(inner1, inner1 - m0, inner1 - m1, color_argb);
            if (frame_pixels) immediate_quad(inner1 - mfr1, inner1 - mfr0, inner1 - m0, inner1 - m1, frame_color);
        }
    }
}

inline
Vector4 get_color_for_button(Button_Theme *theme, Button_State *state, f32 over_factor, f32 pressed_factor) // @Cleanup: Move over_factor and pressed_factor into state!!!
{
    auto surface_color = theme->surface_color;
    surface_color = lerp(surface_color, theme->surface_color_over,  over_factor);
    surface_color = lerp(surface_color, theme->surface_color_down,  state->down_effect_t);
    surface_color = lerp(surface_color, theme->surface_color_flash, pressed_factor);

    return surface_color;
}

//
// Radio buttons & Checkboxes:
//

bool base_checkbox(Rect r, String text, bool selected, Checkbox_Theme *theme = NULL, i64 identifier = 0, Source_Location loc = Source_Location::current())
{
    if (!theme) theme = &default_overall_theme.checkbox_theme;

    auto hash  = ui_get_hash(loc, identifier);

    auto status = get_status_flags(r);
    auto state = find_or_create_state(&table_checkbox, hash);
    defer { stop_using_state(&state->widget); };

    auto font = theme->button_theme.label_theme.font;

    auto text_width  = get_text_width(font, text);
    auto text_height = font->character_height;
    auto box_size    = text_height - 1;

    auto between_pad = em(font, theme->button_theme.label_theme.alignment_pad_ems);

    auto w = r.w;
    auto h = r.h;
    if (w < box_size)    w = text_width + box_size + between_pad;
    if (h < text_height) h = text_height;

    auto box_x = r.x;
    auto box_y = r.y + floorf((h - box_size + 1) * .50f);

    auto vibration = 0.0f; // Get used in the bitmap drawing part.
    {
        // Compute vibration if we have recently been pertubed.
        if (state->base.action_duration_2 >= 0)
        {
            auto theta = TAU * 3 * state->base.action_duration_2;
            auto ct = sinf(theta);

            auto damp = 1 - state->base.action_duration_2 / 2.3f;
            damp = std::clamp(damp, 0.0f, 1.0f);
            damp *= damp;

            if (damp == 0) state->base.action_duration_2 = -1;

            vibration = box_size * ct * .05f * damp;
        }
    }

    auto button_theme = theme->button_theme; // Copy this so we can change colors due to being selected.
    auto button = &button_theme;

    {
        // Update selected_t and modify button and text colors for the selected item.
        auto target = selected ? 1.0f : 0.0f;

        auto denom = button->press_duration;
        if (!denom) denom = 1.0f;

        auto rate = selected ? 1.0f/denom : 1.0f;

        auto dt = ui_current_dt;
        // Actually, let this be slow, so that the color change due to being pressed dominates the experience.
        state->selected_t = move_toward(state->selected_t, target, dt * rate);

        button->surface_color = lerp(button->surface_color, theme->button_color_selected, state->selected_t);
        button->label_theme.text_color = lerp(button->label_theme.text_color, theme->text_color_selected, state->selected_t);
    }

    {
        //
        // The bitmap part.
        //

        auto b = box_size + vibration;
        auto center_x = box_x + box_size*.5f;
        auto center_y = box_y + box_size*.5f;

        Texture_Map *image = NULL;

        if (theme->is_radio_button)
        {
            image = selected ? map_radiobox_full : map_radiobox_empty;
        }
        else
        {
            image = selected ? map_checkbox_full : map_checkbox_empty;
        }

        auto image_rect = get_rect(center_x - b*.5f, center_y - b*.5f, b, b);

        bool changed = status & Status_Flags::PRESSED;
        auto [over_factor, flash_factor] = update_production_value_button(r, changed, &state->base, status, &theme->button_theme);

        Vector2 p0, p1, p2, p3;
        get_quad(image_rect, &p0, &p1, &p2, &p3);

        set_shader(shader_argb_and_texture);
        set_texture(String("diffuse_texture"), image);

        auto image_color = get_color_for_button(button, &state->base, over_factor, flash_factor);

        immediate_quad(p0, p1, p2, p3, argb_color(image_color));
        immediate_flush();

        //
        // The text part.
        //

        r.x += box_size + between_pad;
        r.w -= box_size + between_pad;

        auto text_color = lerp(button->label_theme.text_color, button->text_color_over, over_factor);
        text_color      = lerp(text_color, button->text_color_pressed, flash_factor);

        Label_Theme label_theme = button->label_theme;
        label_theme.text_color = text_color;
        label(r, text, &label_theme);
        // core_draw_label(r, text, text_width, &label_theme, theme->button_theme.alignment, theme->button_theme.alignment_pad_ems);
    }

    auto changed = status & Status_Flags::PRESSED;
    return changed;
}

// Helpful for making checkboxes for enums
#define enum_to_string(enum_value) String(#enum_value)

// @Note: For now we assume the flags passed into here are u32...
// 'flags'      is a parameter that holds the state of the toggled things inside the enum.
// 'flag_value' is a parameter that holds the value to AND with.
bool checkbox_flags(Rect r, String text, u32 *flags, u32 flag_value, Checkbox_Theme *theme = NULL, i64 identifier = 0, Source_Location loc = Source_Location::current())
{
    auto value = ((*flags) & flag_value) != 0;
    if (base_checkbox(r, text, value, theme, identifier, loc))
    {
        *flags ^= flag_value;
        return true;
    }

    return false;
}

//
// Slider:
//

template <typename T>
void set_initial_value(Slider_State *state, T value)
{
    static_assert((std::is_integral<T>::value || std::is_floating_point<T>::value), "Type must be int or float");

    if      (std::is_same<T, f32>::value) state->initial_value_f32 = value;
    else if (std::is_same<T, f64>::value) state->initial_value_f64 = value;
    else    state->initial_value_int = static_cast<i32>(value);
}

template <typename T>
T get_initial_value(Slider_State *state, T dummy_value) // dummy value to probe the type for it
{
    static_assert((std::is_integral<T>::value || std::is_floating_point<T>::value), "Type must be int or float");

    if      (std::is_same<T, f32>::value) return state->initial_value_f32;
    else if (std::is_same<T, f64>::value) return state->initial_value_f64;
    else    return state->initial_value_int;

    assert(0);
}

template <typename T>
void set_and_maybe_clamp(T *value, T override_value, T min_value, T max_value, Slider_Theme *theme)
{
    static_assert((std::is_integral<T>::value || std::is_floating_point<T>::value), "Type must be int or float");

    if (theme->clamp_text_input_low)  override_value = std::max(override_value, min_value);
    if (theme->clamp_text_input_high) override_value = std::min(override_value, max_value);

    *value = override_value;
}

void draw_arrow(Button_State *state, Button_Theme *theme, Vector2 p4, Vector2 p5, Vector2 p6)
{
    auto denom = theme->press_duration;
    if (!denom) denom = 1;
    auto s = state->action_duration / denom;
    s = std::clamp(s, 0.0f, 1.0f);

    // We want s to go from 0 to 1 then to 0
    // Using the parabola equation:
    // f(x) = a*x^2 + b*x + c
    // f(0) = 0  => c = 0
    // f(1) = 0  => a = -b
    // f(.5) = 1 => a*.25 + b*.5 = 1  =>  a*.25 -a*.5 = 1
    //           =>  -.25*a = 1       =>  a = -4   and   b = 4
    // f(x) = -4*x^2 + 4*x
    auto t = -4*s*s + 4*s;

    auto new_p5 = lerp(p5, p6, t);
    auto new_p6 = lerp(p6, p5, t);

    // @Cleanup probably want a different variable since text_color usually mean something else.
    auto arrow_color = theme->label_theme.text_color;
    arrow_color = lerp(arrow_color, theme->text_color_over, state->over_effect_t);
    arrow_color = lerp(arrow_color, theme->text_color_pressed, t);

    immediate_triangle(p4, new_p5, new_p6, argb_color(arrow_color));
};

template <typename T>
bool slider(Rect _r, T *value, T min_value, T max_value, String prefix, String suffix, Slider_Theme *theme, T spinbox_step, bool drag, i64 identifier, Source_Location loc)
{
    if (!theme) theme = &default_overall_theme.slider_theme;

    static_assert((std::is_integral<T>::value || std::is_floating_point<T>::value), "Value type for slider must be int or float");

    bool is_int            = std::is_integral<T>::value;
    bool is_floating_point = std::is_floating_point<T>::value;
    
    auto hash    = ui_get_hash(loc, identifier);
    bool created = false;
    auto state   = find_or_create_state(&table_slider, hash, &created);
    defer { stop_using_state(&state->widget); };

    if (created) set_initial_value(state, *value);

    auto c_prefix = temp_c_string(prefix);
    auto c_suffix = temp_c_string(suffix);
    String text;
    if (!state->inputting_text)
    {
        if (is_int) text = tprint(String("%s%d%s"),   c_prefix, *value, c_suffix);
        else        text = tprint(String("%s%.*f%s"), c_prefix, theme->decimals, *value, c_suffix);
    }

    auto font = theme->button_theme.label_theme.font;
    auto text_width  = get_text_width(font, text);
    auto text_height = font->character_height;

    auto body_rect = _r; // We will probably cut the spinbox off here later...!!!
    auto status = get_status_flags(body_rect);

    if (theme->text_editable)
    {
        // Handling the text input when user clicks anywhere in the slider region, not just the text.
        auto focus = has_focus(body_rect);

        if ((status & Status_Flags::OVER) && (mouse_button_right_state & KSTATE_START) && !state->inputting_text) // @Incomplete mouse_button_left_double_click
        {
            state->inputting_text = true;

            auto input = grab_the_common_text_input(state);
            activate(input);

            input->do_active_widget_add = true;

            String s;
            if (is_int) s = tprint(String("%d"), *value);
            else s = tprint(String("%.*f"), theme->decimals, *value);

            set_text(input, s);
        }

        if (state->inputting_text)
        {
            auto input = am_i_grabbing_the_common_input(state);

            if (!input || !input->active)
            {
                stop_grabbing_the_common_text_input(state);
                state->inputting_text = false;
            }

            if (focus && state->inputting_text)
            {
                Text_Input_Theme text_input_theme = theme->text_input_theme;
                if (!text_input_theme.font) text_input_theme.font = font;
                draw(input, body_rect, &text_input_theme);

                if (input->entered)
                {
                    stop_grabbing_the_common_text_input(state);
                    state->inputting_text = false;

                    bool success = false;
                    T override_value;
                    if (is_int)
                    {
                        auto [result, remainder] = string_to_int(input->text, &success);
                        override_value = result;
                    }
                    else
                    {
                        auto [result, remainder] = string_to_float(input->text, &success);
                        override_value = result;
                    }

                    if (success)
                    {
                        set_and_maybe_clamp(value, override_value, min_value, max_value, theme);
                        return true;
                    }
                }

                return false;
            }
        }
    }

    if (theme->use_spinboxes)
    {
        auto [spinbox_rect, remainder] = cut_right(body_rect, 2 * theme->spinbox_width * body_rect.h);

        body_rect = remainder;
        // Regenerate the status flags because the body rect changed.
        status = get_status_flags(body_rect);

        auto [right_rect, left_rect] = cut_right(spinbox_rect, theme->spinbox_width * body_rect.h);

        auto spinbox_theme = theme->spinbox_theme;
        spinbox_theme.label_theme.font = font;

        spinbox_theme.rectangle_shape.rounding_flags = Rectangle_Rounding_Flags::SOUTHWEST | Rectangle_Rounding_Flags::NORTHWEST; // Only round the left side for the left spinbox.
        auto [pressed_down, state_left] = button(left_rect,  String(""), &spinbox_theme, NULL, hash);

        spinbox_theme.rectangle_shape.rounding_flags = Rectangle_Rounding_Flags::SOUTHEAST | Rectangle_Rounding_Flags::NORTHEAST; // Only round the right side for the right spinbox.
        auto [pressed_up, state_right]  = button(right_rect, String(""), &spinbox_theme, NULL, hash);

        // Draw the triangle for the spinbox buttons.

        set_shader(shader_argb_no_texture);
        {
            auto triangle_margin = right_rect.h * .2f;
            Vector2 p0, p1, p2, p3;

            // Left triangle.
            get_quad(left_rect, &p0, &p1, &p2, &p3);
            auto p4 = lerp(p0, p3, .5f);
            p4.x += triangle_margin;
            auto p5 = p1 + Vector2(-triangle_margin, triangle_margin);
            auto p6 = p2 + Vector2(-triangle_margin, -triangle_margin);

            draw_arrow(state_left, &spinbox_theme, p4, p5, p6);

            // Right triangle.
            get_quad(right_rect, &p0, &p1, &p2, &p3);
            p4 = lerp(p1, p2, .5f);
            p4.x -= triangle_margin;
            p5 = p3 + Vector2(triangle_margin, -triangle_margin);
            p6 = p0 + Vector2(triangle_margin, triangle_margin);

            draw_arrow(state_right, &spinbox_theme, p4, p5, p6);
        }

        auto result = *value;
        if (pressed_up)   result += spinbox_step;
        if (pressed_down) result -= spinbox_step;

        if (pressed_up || pressed_down)
        {
            set_and_maybe_clamp(value, result, min_value, max_value, theme);
        }
    }

    auto started_sliding = false;
    if ((status & Status_Flags::PRESSED) /* @Incomplete: !ui.slider_input_mode */)
    {
        // Anything else could have changed this value, so, when we start sliding,
        // set the initial vlaue so that we know how clamping should behave.
        set_initial_value(state, *value);
        started_sliding = true;
    }

    auto dt = ui_current_dt;
    update_over_state(&state->base, &theme->button_theme, status, dt);
    auto pressed_factor = update_action_durations(&state->base, &theme->button_theme, started_sliding, dt);

    auto over_t = state->base.over_effect_t;
    auto sliding_t_target = 0.0f;
    auto slowing_t_target = 0.0f;

    auto denom = max_value - min_value;
    if (!denom) denom = 1;
    auto t = static_cast<f32>(*value - min_value) / denom;

    if (state->sliding)
    {
        if (!mouse_button_left_state & KSTATE_DOWN) state->sliding = false;
    }
    else
    {
        if (status & Status_Flags::PRESSED) state->sliding = true;
    }

    auto shift_down = ui_button_is_down(CODE_SHIFT); // @Cleanup: Probably want to use ui_get_button_state once we have focus interaction between widgets set up.
    auto surface_margin = body_rect.h * std::min(theme->surface_margin, .49f);
    surface_margin = floorf(surface_margin + .5f);
    if (state->sliding)
    {
        sliding_t_target = 1;

        auto movement_factor = 1.0f;
        if (shift_down)
        {
            // @Responsiveness: slowing_t will be 0 on the first slowing frame, which is generally bad for feeling reactive.
            movement_factor  = .2f;
            slowing_t_target = 1;
        }

        {
            auto denom = (body_rect.w - 2 * surface_margin);
            if (!denom) denom = 1;
            if (drag && !is_int)
            {
                // Relative placement:
                t += mouse_delta_x * movement_factor / denom;
            }
            else
            {
                // Absolute placement:
                t = (mouse_x_float - body_rect.x - 1) / denom;
            }
        }

        // If the initial value started outside the range, well, just
        // don't do the saturate. This is maybe not the best behavior,
        // but we'll probably change this code a lot very soon...
        auto initial = get_initial_value(state, *value);
        if ((min_value <= initial) && (initial <= max_value))
        {
            t = std::clamp(t, 0.0f, 1.0f);
        }

        if (is_int)
        {
            *value = static_cast<i32>(0.5f + lerp(static_cast<f32>(min_value), static_cast<f32>(max_value), t));
            t = static_cast<f32>(*value - min_value) / denom;
        }
        else
        {
            *value = lerp(min_value, max_value, t);
        }
    }

    // We already placed the rect ... we don't way to wident the placement,
    // as that would move other widgets. But, we want to enlarge the whole bar
    // in-place.
    if (state->slowing_t)
    {
        auto dy = em(theme->button_theme.label_theme.font, .25f) * state->slowing_t;
        body_rect.y -= dy;
        body_rect.h += 2 * dy;
    }

    auto qrect = body_rect;
    qrect.x += surface_margin;
    qrect.y += surface_margin;
    qrect.h -= 2 * surface_margin;
    qrect.w = (body_rect.w - 2 * surface_margin) * static_cast<f32>(t);

    if (is_int)
    {
        if (!qrect.w) qrect.w = 1;
    }

    state->sliding_t = move_toward(state->sliding_t, sliding_t_target, dt * 5.f); // :Theme

    if (slowing_t_target)
    {
        state->slowing_t = move_toward(state->slowing_t, slowing_t_target, dt * 4.5f); // :Theme
    }
    else
    {
        state->slowing_t = move_toward(state->slowing_t, slowing_t_target, dt * 15.f); // :Theme
    }

    auto sliding_t = state->sliding_t;

    // Rename the color variables:
    auto slider_color = get_color_for_button(&theme->button_theme, &state->base, over_t, pressed_factor);

    auto background_color = lerp(theme->background_color, theme->background_color_over, over_t);
    background_color      = lerp(background_color, theme->background_color_sliding, sliding_t);
    background_color      = lerp(background_color, theme->background_color_pressed, pressed_factor);

    set_shader(shader_argb_no_texture);
    // Draw the background of the slider.
    if (background_color.w)
    {
        rounded_rectangle(body_rect, theme->button_theme.rectangle_shape, background_color);
    }

    // Draw the foreground of the slider.
    if (slider_color.w)
    {
        rounded_rectangle(qrect, theme->button_theme.rectangle_shape, slider_color);
    }

    Label_Theme label_theme = theme->button_theme.label_theme;

    auto text_color = theme->button_theme.label_theme.text_color;
    text_color = lerp(text_color, theme->button_theme.text_color_over, over_t);
    text_color = lerp(text_color, theme->text_color_sliding, sliding_t);
    text_color = lerp(text_color, theme->button_theme.text_color_pressed, pressed_factor);

    label_theme.text_color = text_color;

    label(body_rect, text, &label_theme);

    immediate_flush();

    return (status & Status_Flags::DOWN);
}

//
// Scrollable region:
//

my_pair<Scrollable_Region_State*, Rect /*inside*/> begin_scrollable_region(Rect r, Scrollable_Region_Theme *theme = NULL, i64 identifier = 0, Source_Location loc = Source_Location::current())
{
    if (!theme) theme = &default_overall_theme.scrollable_region_theme;

    auto rth = render_target_height;

    auto margin_x = floorf(.5f + theme->horizontal_margin_size * rth);
    auto margin_y = floorf(.5f + theme->vertical_margin_size * rth);

    if (!(theme->margin_flags & Margin::HORIZONTAL)) margin_x = 0;
    if (!(theme->margin_flags & Margin::VERTICAL))   margin_y = 0;

    auto scrollbar_width = floorf(.5f + theme->scrollbar_size * rth);
    auto [bar_rect, content_area] = cut_right(r, scrollbar_width);

    auto inside = content_area;
    inside.x += margin_x;
    inside.w -= margin_x * 2;
    inside.y += margin_y;
    inside.h -= margin_y * 2;

    // Draw the background of the inner rect:
    rendering_2d_right_handed();
    set_shader(shader_argb_no_texture);
    immediate_begin();
    rounded_rectangle(content_area, theme->content_area_shape, theme->background_color);
    immediate_flush();
    
    push_scissor(inside);

    auto hash  = ui_get_hash(loc, identifier);
    auto state = find_or_create_state(&table_scrollable_region, hash);
    defer { stop_using_state(&state->widget); };

    state->outer_rect = r;
    state->inner_rect = inside;
    state->bar_rect   = bar_rect;

    return {state, inside};
}

void end_scrollable_region(Scrollable_Region_State *state, f32 min_y, f32 *scroll_value, Scrollable_Region_Theme *theme = NULL)
{
    if (!theme) theme = &default_overall_theme.scrollable_region_theme;

    pop_scissor(); // @Incomplete: Assert that the clip rectangle is owned by us.

    auto inner = state->inner_rect;
    auto bar   = state->bar_rect;
    auto status_inner = get_status_flags(inner);
    auto status_bar   = get_status_flags(bar);

    auto scroll_content_height = std::max(inner.y + inner.h + *scroll_value - min_y, inner.h);
    if (scroll_content_height <= 0.0f) return;

    auto max_scroll_value = scroll_content_height - inner.h;

    auto b = floorf(.5f + theme->nib_margin * bar.w);
    auto min_nib_height = bar.w * theme->minimum_nib_height;

    // @Incomplete: Control whether bar draws with a style flag.
    auto draw_bar = true;
    if (draw_bar)
    {
        set_shader(shader_argb_no_texture);

        auto trench_rect = get_rect(bar.x + b, bar.y + b, bar.w - 2*b, bar.h - 2*b);
        auto trench_color = theme->scrollbar_trench_color;
        rounded_rectangle(bar, theme->scrollbar_shape, trench_color); // Drawing the full bar.
        rounded_rectangle(trench_rect, theme->scrollbar_shape, whiten(trench_color, 0.1f)); // Drawing the inner trench of the bar.

        auto scroll_percent = 0.0f;
        if (max_scroll_value > 0.0f) scroll_percent = *scroll_value / max_scroll_value;

        auto nib_height = std::min(std::max(trench_rect.h * (inner.h / scroll_content_height), min_nib_height), trench_rect.h);
        auto nib_top = lerp(trench_rect.y + trench_rect.h, trench_rect.y + nib_height, scroll_percent);
        auto nib_rect = get_rect(trench_rect.x + b, nib_top - nib_height, trench_rect.w - 2*b, nib_height);

        auto nib_status = get_status_flags(nib_rect);
        auto changed = nib_status & Status_Flags::PRESSED;
        auto [over_factor, pressed_factor] = update_production_value_button(nib_rect, changed, &state->nib_state, nib_status, &theme->scrollbar_nib_theme);

        Vector4 nib_color;
        {
            nib_color = get_color_for_button(&theme->scrollbar_nib_theme, &state->nib_state, over_factor, pressed_factor);
        }

        auto trench_status = get_status_flags(trench_rect);

        if (mouse_button_left_state & KSTATE_START)
        {
            if (nib_status & Status_Flags::OVER)
            {
                state->dragging = true;
            }
            else if (trench_status & Status_Flags::OVER)
            {
                state->teleporting = true;
            }
        }

        if (!(mouse_button_left_state & KSTATE_DOWN))
        {
            state->dragging    = false;
            state->teleporting = false;
        }

        if (nib_height < trench_rect.h)
        {
            if (state->teleporting)
            {
                // Teleport nib to cursor.
                auto p = (trench_rect.y + trench_rect.h - mouse_y_float - nib_height * .5f) / (trench_rect.h - nib_height);
                *scroll_value = lerp(0.0f, max_scroll_value, p);
            }

            if (state->dragging)
            {
                // Drag the scroll nib.
                *scroll_value -= mouse_delta_y * max_scroll_value / (trench_rect.h - nib_height);
                // @Incomplete: // nib_color = nib_color_hilit;
            }
        }

        {
            set_shader(shader_argb_no_texture);
            rounded_rectangle(nib_rect, theme->scrollbar_nib_theme.rectangle_shape, nib_color);
        }
    }

    if (((status_inner & Status_Flags::OVER) || (draw_bar && (status_bar & Status_Flags::OVER))) && mouse_wheel_delta.vertical)
    {
        // Scroll with the mouse wheel.
        auto dz = mouse_wheel_delta.vertical * theme->mouse_wheel_increment * render_target_height;
        *scroll_value -= dz;
    }

    *scroll_value = std::clamp(*scroll_value, 0.0f, max_scroll_value);

    immediate_flush();
}

//
// Dropdown:
//

void dropdown(Rect r, RArr<String> choices, i32 *current_value_pointer, Dropdown_Theme *theme = NULL, i64 identifier = 0, Source_Location loc = Source_Location::current())
{
    // auto old_z = ui_set_z(z); // z is default to 1.
    // defer { ui_set_z(old_z); };

    if (!theme) theme = &default_overall_theme.dropdown_theme;

    // r describes the rect for the top of the dropdown.
    // The individual elements below are sized accroding to theme.button_theme.

    // Draw the background behind the primary choice.

    // Avoid array overbound in case user passes us a dirty value.
    String current_string("");
    auto current_value = *current_value_pointer;
    if ((0 <= current_value) && (current_value < choices.count))
    {
        current_string = choices[current_value];
    }

    auto hash  = ui_get_hash(loc, identifier);
    auto state = find_or_create_state(&table_dropdown, hash);
    defer { stop_using_state(&state->widget); };

    // Carve off the dropdown indicator from the rest of the button.
    auto indicator_width = theme->dropdown_indicator_aspect_ratio * r.h;
    auto [r_right, r_left] = cut_right(r, indicator_width);

    // @Incomplete: This button hack means the button will update twice as fast, so we need to fix that.
    auto button_theme_for_current = theme->theme_for_current_value;
    button_theme_for_current.rectangle_shape.rounding_flags = Rectangle_Rounding_Flags::SOUTHWEST | Rectangle_Rounding_Flags::NORTHWEST;
    auto [pressed, current_value_state] = button(r_left, current_string, &button_theme_for_current, NULL, identifier, loc);

    button_theme_for_current.rectangle_shape.rounding_flags = Rectangle_Rounding_Flags::SOUTHEAST | Rectangle_Rounding_Flags::NORTHEAST;
    auto [indicator_pressed, indicator_state] = button(r_right, String(""), &button_theme_for_current, NULL, identifier, loc); // Same identifier and loc so they press simultaneously.

    if (pressed || indicator_pressed)
    {
        state->open = !state->open;

        if (state->open) active_widget_add(&state->widget);
        // else active_widget_remove(&state->widget);
    }

    // Update open_t for drawing arrows, etc.
    auto dt = ui_current_dt;
    if (state->open)
    {
        state->open_t = move_toward(state->open_t, 1, dt * theme->arrow_flip_up_speed);
    }
    else
    {
        state->open_t = move_toward(state->open_t, 0, dt * theme->arrow_flip_down_speed);
    }

    // Draw the indicator at the right for the dropdown.
    {
        auto triangle_margin = r_right.w * .3f;
        Vector2 p0, p1, p2, p3;

        get_quad(r_right, &p0, &p1, &p2, &p3);
        auto p4 = lerp(p0, p1, .5f);
        p4.y += triangle_margin;
        auto p5 = p2 + Vector2(-triangle_margin, -triangle_margin);
        auto p6 = p3 + Vector2( triangle_margin, -triangle_margin);

        set_shader(shader_argb_no_texture);

        if (state->open_t)
        {
            auto theta = state->open_t * TAU * .5f;
            auto barycenter = (p4 + p5 + p6) * (1 / 3.0f);
            p4 -= barycenter;
            p5 -= barycenter;
            p6 -= barycenter;

            p4 = rotate(p4, theta);
            p5 = rotate(p5, theta);
            p6 = rotate(p6, theta);

            p4 += barycenter;
            p5 += barycenter;
            p6 += barycenter;
        }

        draw_arrow(current_value_state, &button_theme_for_current, p4, p5, p6);
    }    

    if (state->open)
    {
        auto choice_theme = &theme->theme_for_other_choices;
        auto label_other_choices = choice_theme->label_theme;

        auto s = r;
        s.y -= r.h;

        // For now, w eassume the choices are the same height as the current value display.
        i64 pick_choice = -1;
        i64 it_index = 0;
        for (auto other : choices)
        {
            auto sub_hash = ui_get_hash(Source_Location::current(), it_index);
            i64 choice_hash = static_cast<i64>(combine_hashes(hash, sub_hash));

            auto t = choice_theme;
            if (static_cast<i32>(it_index) == *current_value_pointer) t = &theme->theme_for_current_choice;
            auto [other_pressed, other_choice_state] = button(s, other, t, NULL, choice_hash, loc);

            s.y -= s.h;

            if (other_choice_state->released) // Pick on released!
            {
                pick_choice = it_index;
            }

            it_index += 1;
        }

        if (pick_choice >= 0)
        {
            state->open = false;
            *current_value_pointer = static_cast<i32>(pick_choice);
            update_action_durations(current_value_state, &button_theme_for_current, true, 0);
        }
    }

    immediate_flush();
}

void dropdown_deactivate(Active_Widget *widget)
{
    auto state = CastDown_Widgets<Dropdown_State>(widget);

    if (!state->open) return;

    state->open = false;
}

//
// Slidable region:
//

inline
void compute_slide_regions(Rect r, Slidable_Region_State *state, Rect *left_or_top_rect, Rect *divider_rect, Rect *right_or_bottom_rect)
{
    auto theme = state->theme;
    auto divider_thickness = get_float_parameter(r, theme->divider_thickness_type, theme->divider_thickness, theme->divider_thickness);
    auto half_divider_thickness = floorf(.5f + divider_thickness*.5f);

    if (theme->orientation == Slidable_Region_Theme::HORIZONTAL)
    {
        auto x = r.w * state->divider_t - half_divider_thickness;
        x = std::clamp(x, 0.0f, r.w - divider_thickness);

        auto [left, middle]    = cut_left(r, x);
        auto [div_rect, right] = cut_left(middle, divider_thickness);

        *left_or_top_rect     = left;
        *right_or_bottom_rect = right;
        *divider_rect         = div_rect;
    }
    else
    {
        auto y = r.h * state->divider_t - half_divider_thickness;
        y = std::clamp(y, 0.0f, r.h - divider_thickness);

        auto [bottom, middle] = cut_bottom(r, y );
        auto [div_rect, top]   = cut_bottom(middle, divider_thickness);

        *left_or_top_rect     = top;
        *right_or_bottom_rect = bottom;
        *divider_rect         = div_rect;
    }
}

inline
f32 get_float_parameter(Rect r, Size_Relativeness relativeness, f32 local_value, f32 fixed_value)
{
    f32 result = 0;
    switch (relativeness)
    {
        case RELATIVE_TO_HEIGHT: {
            result = r.h * local_value;
        } break;
        case RELATIVE_TO_WIDTH: {
            result = r.w * local_value;
        } break;
        case ABSOLUTE_FROM_THEME_FIELD:
            result = fixed_value;
        case ABSOLUTE_FROM_GLOBAL: {
            auto k = render_target_height * .1f; // Arbitrary unit that maps with the render target height.
            result = fixed_value * k; // @Cleanup: We should have a global variable for this. @Temporary.
        } break;
        default: assert(0);
    }

    return result;
}

Slidable_Region_State *begin_slidable_region(Rect r, Slidable_Region_Theme *theme = NULL, i64 identifier = 0, Source_Location loc = Source_Location::current())
{
    if (!theme) theme = &default_overall_theme.slidable_region_theme;

    auto hash    = ui_get_hash(loc, identifier);
    bool created = false;
    auto status  = get_status_flags(r);
    auto state   = find_or_create_state(&table_slidable_region, hash, &created);
    defer { stop_using_state(&state->widget); };

    //
    // Get the divider dimension and also maybe get it's position in the rect.
    //
    f32 divider_dimension = 0;
    Vector2 divider_vector;
    if (theme->orientation == Slidable_Region_Theme::HORIZONTAL)
    {
        divider_dimension = r.w;
        divider_vector = Vector2(1, 0);
    }
    else
    {
        divider_dimension = r.h;
        divider_vector = Vector2(0, 1);
    }
    
    if (created)
    {
        f32 divider_offset_in_pixels = get_float_parameter(r, theme->initial_slider_position_type, theme->initial_slider_position, theme->initial_slider_position); // @Cleanup: We should have a global variable for this.

        auto denom = divider_dimension;
        if (!denom) denom = 1;
        state->divider_t = divider_offset_in_pixels / denom;
    }

    //
    // We compute the divider region, the left_or_top region, the right_or_bottom region.
    // We may do this computation twice, becase we want to handle the input in a low-latency
    // and accurate way! If we move the divider, we'll call this again.
    //
    Rect left_or_top_rect;
    Rect divider_rect;
    Rect right_or_bottom_rect;

    state->theme = theme; // Setting the theme here because compute_slide_regions needs it.
    compute_slide_regions(r, state, &left_or_top_rect, &divider_rect, &right_or_bottom_rect);

    //
    // Do some dragging for the divider rect.
    //
    auto divider_status = get_status_flags(divider_rect);
    auto changed = false;
    if (divider_status & Status_Flags::OVER)
    {
        if (mouse_button_left_state & KSTATE_START)
        {
            state->dragging = true;
            changed = true;
        }
    }

    auto focus = has_focus(divider_rect);
    auto down = focus && (mouse_button_left_state & KSTATE_DOWN);
    if (state->dragging)
    {
        if (down)
        {
            auto dir = Vector2(mouse_delta_x, mouse_delta_y);
            auto slided_xy = divider_vector * glm::dot(dir, divider_vector);

            // One of the coordinates of 'slided_xy' will be 0, so let's just add them.
            // If it is zero, then it didn't move.
            auto moved = slided_xy.x + slided_xy.y;
            if (moved)
            {
                auto denom = divider_dimension;
                if (!denom) denom = 1;
                state->divider_t += moved / denom;
                state->divider_t = std::clamp(state->divider_t, 0.0f, 1.0f);

                // Recompute the rects since the dividewr moved!
                compute_slide_regions(r, state, &left_or_top_rect, &divider_rect, &right_or_bottom_rect);
            }
        }
        else
        {
            state->dragging = false;
        }
    }

    //
    // Draw the background.
    //
    {
        rendering_2d_right_handed();
        set_shader(shader_argb_no_texture);

        rounded_rectangle(r, theme->background_shape, theme->background_color);
        immediate_flush();
    }

    state->left_or_top_rect     = left_or_top_rect;
    state->right_or_bottom_rect = right_or_bottom_rect;
    state->divider_rect         = divider_rect;

    // We are fine doing this at the start of the slidable region because so supposed to call this
    // procedure and the end_slidable_region() in the same frame. So the divider over factor and
    // pressed factor are valid.
    auto [divider_over_factor, divider_pressed_factor] = update_production_value_button(divider_rect, changed, &state->divider_state, divider_status, &theme->divider_theme);

    state->divider_over_factor    = divider_over_factor;
    state->divider_pressed_factor = divider_pressed_factor;

    push_scissor(state->left_or_top_rect);

    return state;
}

void switch_to_right_or_bottom_rect(Slidable_Region_State *state)
{
    pop_scissor(); // @Incomplete: Ensure that this is the right scissor.
    push_scissor(state->right_or_bottom_rect);
}

void switch_to_left_or_top_rect(Slidable_Region_State *state)
{
    // We set this up already in begin_slidable_region.
    pop_scissor(); // @Incomplete: Ensure that this is the right scissor.
    push_scissor(state->left_or_top_rect);
}

void end_slidable_region(Slidable_Region_State *state)
{
    pop_scissor(); // @Incomplete: Assert that the clip rectangle is owned by us.

    auto div_theme = &state->theme->divider_theme;
    auto div_color = get_color_for_button(div_theme, &state->divider_state, state->divider_over_factor, state->divider_pressed_factor);
    rounded_rectangle(state->divider_rect, div_theme->rectangle_shape, div_color);

    immediate_flush();
}

inline
Texture_Map *require_texture(String file)
{
    auto result = catalog_find(&texture_catalog, file);
    // If the catalog was not able to find the texture, the error would be logged in catalog_find().
    assert(result); // This is to stop the program in case of load fails.

    return result;
}

void init_ui(Dynamic_Font *default_font)
{
    Overall_Theme dummy;
    default_overall_theme = dummy; // We need to do this because the font for some of the fields by default could be NULL, since at that time, we have not set the value for default_editor_font yet.

    // @Note: We are not setting the font for the themes here,
    // because when you create a new theme, you supposed to
    // default initialize the font member with 'default_editor_font'.
    // See struct Label_Theem.

    // Load the bitmaps.
    map_radiobox_full  = require_texture(String("radio_full"));
    map_radiobox_empty = require_texture(String("radio_empty"));
    map_checkbox_full  = require_texture(String("checkbox_full"));
    map_checkbox_empty = require_texture(String("checkbox_empty"));
}

void ui_per_frame_update(f32 dt)
{
    assert((ui_state_stack.count == 0));

    auto [_mouse_x, _mouse_y] = render_target_mouse_pointer_position(glfw_window, true);
    mouse_x = _mouse_x;
    mouse_y = _mouse_y;

    mouse_x_float = static_cast<f32>(mouse_x);
    mouse_y_float = static_cast<f32>(mouse_y);

    mouse_button_left_state  = ui_get_button_state(CODE_MOUSE_LEFT);
    mouse_button_right_state = ui_get_button_state(CODE_MOUSE_RIGHT);

    ui_current_dt = dt;

    if (was_window_resized_this_frame)
    {
        default_editor_font = get_font_at_size(FONT_FOLDER, String("KarminaBold.otf"), BIG_FONT_SIZE * .8f);
        cursive_font = get_font_at_size(FONT_FOLDER, String("GreatVibes-Regular.ttf"), BIG_FONT_SIZE * 3.5f);

        // @Copypasta from init_ui @Cleanup
        Overall_Theme dummy;
        default_overall_theme = dummy;
    }

    popups_per_frame_update();
    occlusion_per_frame_update();
}

void ui_handle_event(Event event)
{
    // Search all Text_Inputs for one to activate.

    // @Speed: All events are currently considered to be in the same place,
    // but we can handle each Text_Input each time. This can of course be
    // speed up drastically.
    // Solution is: make a _by_text_input array like in entity_manager.cpp....
    for (auto &it : table_text_input)
    {
        auto text_input_state = &it.value;
        if (!is_inside(mouse_x_float, mouse_y_float, text_input_state->rect)) continue;

        if (!is_inside(mouse_x_float, mouse_y_float, text_input_state->rect)) continue;
        using_state_to_check_for_activating_text_input(text_input_state, event);
    }

    if (ui_active_widget && cmp_var_type_to_type(ui_active_widget->widget_type, Text_Input_State)) // Only handle events for the current active Text_Input.
    {
        if (ui_active_widget->event_proc)
        {
            ui_active_widget->event_proc(ui_active_widget, event);
        }
    }
}

/*
void draw_view_area(i32 x0, i32 area_width) // Currently not using
{
    assert((x0 == 0)); // We don't take into account non-zero x0 in the code below yet.

    String text("Hello sailor!");
    auto text_width = get_text_width(default_editor_font, text);

    Rect r;
    r.x = 200;
    r.y = render_target_height * 0.5f;
    r.w = text_width * 1.8f;
    r.h = default_editor_font->character_height * 4.0;

    auto theme = default_overall_theme.button_theme;

    button(r, text, &theme, 0);

    r.y -= r.h * 1.1f;
    button(r, String("7777 is my fav number."), &theme, 1);
}

Dynamic_Font *mode_button_font = NULL; // @Temporary: Using this until we do the bitmap icons for the modes' buttons.
Dynamic_Font *widget_button_font = NULL; // @Temporary: Using this until we do the bitmap icons for the modes' buttons.
Contents_Mode contents_mode;

void update_mode_column_fonts(f32 width)  // Currently not using :LayoutUI
{
    {
        if (mode_button_font) return;
        auto font_size = static_cast<i32>(width / 4 + 0.5f);

        // @Leak: Not freeing the old font.
        if (mode_button_font) logprint("update_mode_column_fonts", "Leaking the old mode button font...\n");
        mode_button_font = get_font_at_size(FONT_FOLDER, String("KarminaBoldItalic.otf"), font_size);
    }

    {
        if (widget_button_font) return;
        auto font_size = static_cast<i32>(width / 2.9f + 0.5f);

        // @Leak: Not freeing the old font.
        if (widget_button_font) logprint("update_mode_column_fonts", "Leaking the old widget button font...\n");
        widget_button_font = get_font_at_size(FONT_FOLDER, String("KarminaBoldItalic.otf"), font_size);
    }
}

void set_contents_mode(Contents_Mode mode)  // Currently not using :LayoutUI
{
    // We probably will do some visual effects here when we change the contents mode.

    contents_mode = mode;
}

void draw_contents_area_widgets(Rect rect_contents)  // Currently not using :LayoutUI
{
    auto margin_total = rect_contents.w * .2f;
    auto widget_button_width  = (rect_contents.w - margin_total) * .5f;
    auto widget_button_height = widget_button_width * (10 / 16.0f);

    //
    // Two columns of widget buttons.
    //

    auto outer_margin  = margin_total * (2 / 5.0f);
    auto center_margin = margin_total - 2*outer_margin;
    auto vertical_margin = outer_margin;

    Rect widget_a;
    widget_a.x = rect_contents.x + outer_margin;
    widget_a.w = widget_button_width;
    widget_a.y = rect_contents.y + rect_contents.h - 2*outer_margin;
    widget_a.h = widget_button_height;
    
    auto widget_b = widget_a;
    widget_b.x = widget_a.x + widget_a.w + center_margin;

    auto theme = default_overall_theme.button_theme;
    theme.font = widget_button_font;

    bool pressed;

    widget_a.y -= vertical_margin + widget_button_height;
    widget_b.y -= vertical_margin + widget_button_height;
    pressed = button(widget_a, String("Labels"), &theme);
    if (pressed) set_contents_mode(Contents_Mode::Widget_Place_Labels);
    pressed = button(widget_b, String("Push Buttons"), &theme);
    if (pressed) set_contents_mode(Contents_Mode::Widget_Place_Push_Buttons);

    widget_a.y -= vertical_margin + widget_button_height;
    widget_b.y -= vertical_margin + widget_button_height;
    pressed = button(widget_a, String("Check Buttons"), &theme);
    if (pressed) set_contents_mode(Contents_Mode::Widget_Place_Check_Buttons);
    pressed = button(widget_b, String("Text Inputs"), &theme);
    if (pressed) set_contents_mode(Contents_Mode::Widget_Place_Text_Inputs);
}

void draw_contents_area(Rect rect_contents)  // Currently not using :LayoutUI
{
    switch (contents_mode)
    {
        case Contents_Mode::Widgets: draw_contents_area_widgets(rect_contents); break;
        case Contents_Mode::Layouts:
        {
        } break;
        case Contents_Mode::Inputs:
        {
        } break;
        default: break;
    }
}

void draw_control_area(i32 x0, i32 area_width)  // Currently not using :LayoutUI
{
    auto y0 = 0.0f;

    Rect mode_column_r;
    mode_column_r.x = x0;
    mode_column_r.y = y0;
    mode_column_r.w = area_width * .17f;
    mode_column_r.h = render_target_height;

    Rect contents_r;
    contents_r.x = x0 + mode_column_r.w;
    contents_r.y = y0;
    contents_r.w = area_width - mode_column_r.w;
    contents_r.h = render_target_height;

    auto mode_column_color  = Vector4(.24, .00, .00, 1.0);
    auto contents_color     = Vector4(.3, .02, .02, 1.0);

    Vector2 p0, p1, p2, p3;

    set_shader(shader_argb_no_texture);

    get_quad(mode_column_r, &p0, &p1, &p2, &p3);
    auto c = mode_column_color;
    draw_quad(p0, p1, p2, p3, c);

    get_quad(contents_r, &p0, &p1, &p2, &p3);
    switch (contents_mode)
    {
        case Contents_Mode::Widgets: contents_color = Vector4(.3, .02, .02, 1.0); break;
        case Contents_Mode::Layouts: contents_color = Vector4(.2, .2, .7, 1.0); break;
        case Contents_Mode::Inputs:  contents_color = Vector4(.7, .3, .7, 1.0); break;
        default: Vector4(.3, .02, .02, 1.0);
    }
    c = contents_color;
    draw_quad(p0, p1, p2, p3, c);

    //
    // Draw the buttons for the different modes.
    //
    auto mode_button_width = mode_column_r.w * .9;
    update_mode_column_fonts(mode_button_width);

    auto mode_button_margin = (mode_column_r.w - mode_button_width) * .5f;
    auto mode_button_top_margin = mode_button_margin;

    auto theme = default_overall_theme.button_theme;
    theme.font = mode_button_font;

    Rect r;
    r.x = mode_column_r.x + mode_button_margin;
    r.y = mode_column_r.y + mode_column_r.h - mode_button_margin;
    r.w = mode_button_width;
    r.h = mode_button_width;

    bool pressed;

    r.y -= mode_button_top_margin + mode_button_width;
    pressed = button(r, String("Widgets"), &theme);
    if (pressed) set_contents_mode(Contents_Mode::Widgets);

    r.y -= mode_button_top_margin + mode_button_width;
    pressed = button(r, String("Layouts"), &theme);
    if (pressed) set_contents_mode(Contents_Mode::Layouts);

    r.y -= mode_button_top_margin + mode_button_width;
    pressed = button(r, String("Inputs"), &theme);
    if (pressed) set_contents_mode(Contents_Mode::Inputs);

    draw_contents_area(contents_r);
}
*/

// @Temporary: Testing data structures to help the development of the Widgets :DeprecateMe
struct Grocery_Info
{
    String name;
    bool should_get = false;
};

RArr<Grocery_Info> groceries;

RArr<String> spells;
i32 current_spell = -1;

enum Damage_Resistance
{
    EMOTIONAL  = 1 << 0,
    ELECTRICAL = 1 << 1,
    ELEMENTAL  = 1 << 2,
    POISON     = 1 << 3
};
u32 damage_resistance = 0;

bool first_time = true; // @Temporary:
void draw_ui()
{
    auto dt = timez.ui_dt;
    ui_per_frame_update(dt);
    show_os_cursor(glfw_window);

    auto kx = render_target_height * .1f; // 'kx' is an arbitrary value based on the render target height that will ensure the ui not be pixel dependence.

    if (first_time) //  @Temporary @Hack :DeprecateMe
    {
        first_time = false;
        init_ui(default_editor_font);

        {
            Grocery_Info g;

            g.name = String("Apples");
            array_add(&groceries, g);

            g.name = String("Kiwis");
            array_add(&groceries, g);

            g.name = String("Bananas");
            array_add(&groceries, g);

            g.name = String("Chai Juice");
            array_add(&groceries, g);
        }

        {
            array_add(&spells, String("fire"));
            array_add(&spells, String("water"));
            array_add(&spells, String("toxic"));
            array_add(&spells, String("lighting"));
        }

        {
            set_text(&text_input_a, String("Lord dimwit flathead!"));
            set_text(&text_input_b, String("Flood control Dam #3."));
            set_text(&text_input_c, String("Holy cowabunga!"));
            set_auto_complete(&text_input_d, auto_complete_teas, NULL);
        }

        {
            array_add(&awesome_names, String("Holy Moldy"));
            array_add(&awesome_names, String("Stuart Schmidt"));
            array_add(&awesome_names, String("Commander Bear"));
            array_add(&awesome_names, String("Buggy Fungi"));
            array_add(&awesome_names, String("Chompy Clap"));
            array_add(&awesome_names, String("How about now"));
            array_add(&awesome_names, String("Chaos!!!"));
        }

        my_subwindow_a = get_rect(kx * 2.7f, kx * 1.1f, kx * 6.f, kx * 5.f);
        my_subwindow_b = get_rect(kx * 1.f + my_subwindow_a.x + my_subwindow_b.w, kx * 2.1f, my_subwindow_a.w, my_subwindow_a.h);
    }

    {
        // Vanilla button demo, with some scissoring.
        String text("Hello sailor!");
        auto text_width = get_text_width(default_editor_font, text);

        Rect r = get_rect(kx * .3f, kx * 2.5f, kx * 3.f, default_editor_font->character_height * 3.5);

        auto theme = default_overall_theme.button_theme;
        theme.label_theme.text_baseline_vertical_position = .15f;
        theme.label_theme.alignment = Text_Alignment::Left;
        button(r, text, &theme);

        r.y -= r.h * 1.1f;
        theme.label_theme.alignment = Text_Alignment::Center;
        button(r, String("7777 is my fav number."), &theme);

        r.y -= r.h * 1.1f;
        theme.label_theme.alignment = Text_Alignment::Right;
        button(r, String("Da bomb."), &theme);
    }

    {
        Rect r;
        {
            // Default checkbox demo.
            auto font = default_editor_font;
            r = get_rect(kx * 3.5f, kx * 2.3f, kx * 1.5f, font->character_height);
            auto old_y = r.y;

            auto vertical_pad = r.h * .1f;

            auto it_index = 0;
            for (auto &it : groceries)
            {
                auto pressed = base_checkbox(r, it.name, it.should_get, NULL, it_index);
                it.should_get ^= pressed;
                r.y -= font->character_height + vertical_pad;

                it_index += 1;
            }

            // Radio buttons demo.
            r = get_rect(r.x + r.w + vertical_pad + kx*.3f, old_y, kx * 1.3f, font->character_height);

            auto radio_theme = default_overall_theme.checkbox_theme;
            radio_theme.is_radio_button = true;

            it_index = 0;
            for (auto it : spells)
            {
                auto selected = (it_index == current_spell);
                auto pressed  = base_checkbox(r, it, selected, &radio_theme, it_index);

                if (pressed) current_spell = it_index;

                r.y -= font->character_height;

                it_index += 1;
            }
        }

        // Text input demo.
        {
            auto text_color = Vector4(1, 1, 0, 1);

            // Demo left justified.
            r = get_rect(kx * 2.6f, kx * 7.5f, kx * 5.f, default_editor_font->character_height * 1.25f);
            auto input_a_rect = r;
            draw(&text_input_a, r);

            Text_Input_Theme text_theme = default_overall_theme.text_input_theme;

            // Demo center justified.
            text_theme.alignment = Text_Alignment::Center;
            r.y -= r.h * 1.2f;
            draw(&text_input_b, r, &text_theme);

            // Demo right justified.
            text_theme.alignment = Text_Alignment::Right;
            r.y -= r.h * 1.2f;
            draw(&text_input_c, r, &text_theme);

            // Demo auto complete.
            text_theme.alignment = Text_Alignment::Left;
            r.x = kx * 12.f;
            r.y = input_a_rect.y;
            draw(&text_input_d, r, &text_theme);
        }

        // Demo sliders.
        {
            // Floating point slider.
            auto slider_rect = get_rect(kx * 7.9f, kx * 7.6f, kx * 3.8f, kx * .33f);
            slider(slider_rect, &slider_floating_point_value, 0.0f, 100.0f, String(""), String("fps"));

            // Integer point slider.
            slider_rect.y -= slider_rect.h * 1.2f;
            slider(slider_rect, &slider_integral_value, 0, 10, String("number "), String(""));
        }

        // Demo scrollable region.
        {
            r = get_rect(kx * 7.f, kx * .3f, kx * 3.5f, kx * 3.f);

            Scrollable_Region_Theme scroll_theme;
            auto [region_state, inner_rect] = begin_scrollable_region(r, &scroll_theme);

            constexpr i32 NUM_BUTTONS = 30;
            auto button_height = floorf(.5f + inner_rect.w * .15);

            Button_Theme button_theme;
            button_theme.label_theme.alignment = Text_Alignment::Right;

            // @Incomplete: Add the scrolling offset when we have it.
            // @Incomplete: Scroll value is in pixels, what happnes if the screen changes resolution?
            r = inner_rect;
            r.y = r.y + r.h - button_height;
            r.h = button_height;
            r.y += demo_scroll_value;
            for (i32 i = 1; i <= NUM_BUTTONS; ++i)
            {
                auto text = tprint(String("Button %2d"), i);
                button(r, text, &button_theme, NULL, i);

                if (i != NUM_BUTTONS) r.y -= button_height * 1.1f; // Just so that we don't have a missing slot for buttons.
            }

            end_scrollable_region(region_state, r.y, &demo_scroll_value, &scroll_theme);
        }

        // Demo dropdown.
        {
            r = get_rect(kx * 13.5f, kx * 6.f, kx * 3.f, kx * .4f);
            dropdown(r, awesome_names, &current_name_choice);
        }

        // Demo color picker.
        {
            r = get_rect(kx * 13.2f, kx * .5f, kx * 3.7f, kx * 5.0f);
            Vector3 in_out_color_rgb = Vector3(0, 0, 0);
            auto applied = color_picker(r, &in_out_color_rgb);
        }

        // Demo slidable region.
        {
            r = get_rect(kx * .1f, kx * 4.f, kx * 2.2f, kx * 4.8f);
            Slidable_Region_Theme theme;
            theme.orientation = Slidable_Region_Theme::VERTICAL;
            auto state = begin_slidable_region(r, &theme);
            auto right_or_bottom_rect = state->right_or_bottom_rect;

            // Enum flags checkbox demo.
            {
                auto theme = default_overall_theme.checkbox_theme;
                auto font = default_editor_font;

                auto [region, junk] = cut_top(state->left_or_top_rect, font->character_height);
                auto xpad = region.h * .3f;
                region.y -= xpad * 2;
                region.x += xpad;
                region.w -= xpad * 2;

                region.x = floorf(.5f + region.x);
                region.y = floorf(.5f + region.y);
                region.w = floorf(.5f + region.w);
                region.h = floorf(.5f + region.h);

                checkbox_flags(region, enum_to_string(EMOTIONAL), &damage_resistance, Damage_Resistance::EMOTIONAL, &theme);
                region.y -= font->character_height + xpad;

                checkbox_flags(region, enum_to_string(ELECTRICAL), &damage_resistance, Damage_Resistance::ELECTRICAL, &theme);
                region.y -= font->character_height + xpad;

                checkbox_flags(region, enum_to_string(ELEMENTAL), &damage_resistance, Damage_Resistance::ELEMENTAL, &theme);
                region.y -= font->character_height + xpad;

                checkbox_flags(region, enum_to_string(POISON), &damage_resistance, Damage_Resistance::POISON, &theme);
                region.y -= font->character_height + xpad;
            }

            switch_to_right_or_bottom_rect(state);

            {
                r = right_or_bottom_rect;
                auto state = begin_slidable_region(r);

                switch_to_right_or_bottom_rect(state);

                end_slidable_region(state);
            }

            end_slidable_region(state);
        }

        // Demo subwindow.
        {
            // @Incomplete: Instead of doing this, we should make subwindow be rendered as popups so that we can switch between different ones.

            if (was_window_resized_this_frame)
            {
                my_subwindow_a = get_rect(kx * 2.7f, kx * 1.1f, kx * 6.f, kx * 5.f);
                my_subwindow_b = get_rect(kx * 1.f + my_subwindow_a.x + my_subwindow_b.w, kx * 2.1f, my_subwindow_a.w, my_subwindow_a.h);
            }

            {
                r = my_subwindow_a;
                auto subwindow_state = begin_subwindow(r);
                auto content_area = subwindow_state->content_area;
                my_subwindow_a = end_subwindow(subwindow_state, String("This is window A!"));
            }

            {
                r = my_subwindow_b;
                auto subwindow_state = begin_subwindow(r);
                auto content_area = subwindow_state->content_area;
                my_subwindow_b = end_subwindow(subwindow_state, String("This is window B!"));
            }
        }

        // Demo label (drawing the title over everything)
        {
            Label_Theme theme;
            theme.font = cursive_font;
            theme.text_color = Vector4(.90, .70, .90, 1);

            auto label_height = cursive_font->character_height * 1.5f;
            auto label_rect = get_rect(0, render_target_height - label_height, render_target_width, label_height);
            
            label(label_rect, String("Sokoban!"), &theme);
        }

        draw_popups();

        return;
    }

/*  Figure out the layout for ui :LayoutUI

    auto view_area_width    = render_target_width * .8f;
    auto control_area_width = render_target_width - view_area_width;

    Rect r_view;
    r_view.w = view_area_width;
    r_view.h = render_target_height;

    push_scissor(r_view);
    draw_view_area(0, static_cast<i32>(view_area_width));
    pop_scissor();

    Rect r_control;
    r_control.x = view_area_width;
    r_control.w = control_area_width;
    r_control.h = render_target_height;

    push_scissor(r_control);
    draw_control_area(static_cast<i32>(view_area_width), static_cast<i32>(control_area_width));
    pop_scissor();
*/
}
