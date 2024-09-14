#include "subwindow.h"

#include "draw.h" // For render_target_height, set_shader(), ... 
#include "events.h" // For KSTATE_*.

Subwindow_State *begin_subwindow(Rect r, Subwindow_Theme *theme, i64 identifier, Source_Location loc)
{
    auto hash  = ui_get_hash(loc, identifier);
    auto state = find_or_create_state(&table_subwindow, hash);
    defer { stop_using_state(&state->widget); };

    if (!theme) theme = &default_overall_theme.subwindow_theme;
    state->theme     = *theme; // Make a copy for the state because who knows, you maybe changing stuff around after calling begin_subwindow.
    state->hash      = hash; // What for ??? nocheckin
    state->full_rect = r;

    if (state->num_begins == 0)
    {
        state->num_begins += 1;
    }
    else if (state->num_begins == 1)
    {
        logprint("subwindow", "begin_subwindow was called too many times on this state, without calling end_subwindow first!\n");
    }
    else
    {
        logprint("subwindow", "the 'state' argument passed into begin_subwindow is corrupted!\n");
        return state;
    }

    if (state->dragging)
    {
        if (ui_active_widget != &state->widget)
        {
            state->dragging = Subwindow_State::NOTHING;
        }

        if (!(mouse_button_left_state & KSTATE_DOWN))
        {
            state->dragging = Subwindow_State::NOTHING;
        }

        if (!state->dragging)
        {
            active_widget_remove(&state->widget);
        }
    }

    if (state->dragging)
    {
        auto dx = mouse_x_float - state->dragging_last_x;
        auto dy = mouse_y_float - state->dragging_last_y;

        if (dx || dy)
        {
            if (state->dragging == Subwindow_State::TITLE)
            {
                state->full_rect.x += dx;
                state->full_rect.y += dy;
            }
            else if (state->dragging == Subwindow_State::TO_RESIZE)
            {
                assert(0); // @Incomplete:
            }

            state->dragging_last_x = mouse_x_float;
            state->dragging_last_y = mouse_y_float;
        }
    }

    auto title_bar_height = floorf(.5f + theme->title_bar_height * render_target_height);
    auto [title_bar_rect, content_area] = cut_top(state->full_rect, title_bar_height);

    state->title_bar_rect = title_bar_rect;
    state->content_area   = content_area;

    // Draw the background quad.
    set_shader(shader_argb_no_texture);
    rounded_rectangle(content_area, theme->content_area_shape, theme->content_area_background_color);

    push_scissor(content_area);

    return state;
}

Rect end_subwindow(Subwindow_State *state, String title_bar_string)
{
    if (state->num_begins == 0)
    {
        logprint("subwindow", "end_subwindow was called too many times on this state, or begin was never called.\n");
        return {};
    }
    else if (state->num_begins == 1)
    {
        state->num_begins -= 1;
    }
    else
    {
        logprint("subwindow", "the 'state' argument passed into end_subwindow is corrupted!!!!!\n");
        return {};
    }

    auto theme = &state->theme;
    pop_scissor();

    if (state->dragging)
    {
        if (ui_active_widget != &state->widget) state->dragging = Subwindow_State::NOTHING;
        if (!state->dragging) active_widget_remove(&state->widget);
    }

    auto draw_title_bar = true; // @Incomplete: @Theme: Control whether title bar draws with a style flag.
    if (draw_title_bar)
    {
        auto sub_hash = ui_get_hash(Source_Location::current(), 0);
        auto title_bar_hash = combine_hashes(state->hash, sub_hash);

        auto [pressed, title_bar_state] = button(state->title_bar_rect, title_bar_string, &theme->title_bar_theme, NULL, title_bar_hash);
        if (pressed)
        {
            state->dragging = Subwindow_State::TITLE;
            state->dragging_last_x = mouse_x_float;
            state->dragging_last_y = mouse_y_float;

            // Since it is a click, make the subwindow active.
            active_widget_add(&state->widget);
        }
    }

    // occlusion_declare(state->full_rect, state);

    return state->full_rect;
}
