#pragma once

#include "common.h"
#include "ui.h" // For struct Rect, Active_Widget.

struct Button_State
{
    Active_Widget widget;

    u64 hash = 0;
    f32 over_duration = 0.0f;
    f32 over_effect_t = 0.0f;

    f32 down_duration = 0.0f;
    f32 down_effect_t = 0.0f;

    f32 action_duration = -1.0f; // Set to 0 when an action first happens.
    f32 action_duration_2 = -1.0f; // Set to 0 when an action first happens.

    bool pressed  = false;  // @Cleanup: This flag handling stuff need not be this complicated.
    bool released = false;  // @Cleanup: This flag handling stuff need not be this complicated.
};

struct Column_Button_State // Not used
{
    Active_Widget widget;

    Button_State base;
    f32 open_t = 0; // Ranges from 0 to 1.
};

struct Checkbox_State
{
    Active_Widget widget;

    Button_State base;
    f32 selected_t = 0.0f;
};

struct Text_Input_State
{
    Active_Widget widget;

    Button_State base; // Not all of these members are used (only for over_factor and flash_factor).

    // In text input, I think of the text input region as a camera that slides left/right
    // to follow the movement of the cursor. This is because we want to have an easy notion
    // to visualize the movement of the text input and how to clamp things to the margin.
    bool camera_initted = false;
    f32 camera_x = 0.0f; // This is the left edge of the camera slider pane.

    f32 shrunken_deadzone_margin = -1.0f;
    f32 last_displayed_text_x = FLT_MAX; // FLT_MAX means disabled.

    f32 active_t = 0.0f;

    Text_Input *input = NULL; // Becase we are handling all events of text input within ui_handle_event, we need a way to convert from the states in the states table to the Text_Input themselves, so we have to store a field of Text_Input inside its state. This may not be the best solution but it is what I'm thinking of.

    Rect rect; // The region of the text input in order to handle events (see ui_handle_event).

    Text_Input_State()
    {
        widget.event_proc      = this_handle_event_is_not_the_same_as_the_normal_text_input_handle_event;
        widget.deactivate_proc = this_deactivate_is_not_the_same_as_the_normal_text_input_deactivate;
    }
};

struct Slider_State
{
    Active_Widget widget;

    Button_State base;

    // Initial value is used so that we don't clamp it in the case of a relative drag.
    union
    {
        f32 initial_value_f32;
        f64 initial_value_f64;
        i64 initial_value_int;
    };

    f32 sliding_t = 0;
    f32 slowing_t = 0;

    bool sliding = false;

    bool inputting_text = false;
};

struct Scrollable_Region_State
{
    Active_Widget widget;

    Button_State nib_state;

    Rect outer_rect;
    Rect inner_rect;
    Rect bar_rect;

    bool dragging = false;
    bool teleporting = false;
};

struct Slidable_Region_Theme;
struct Slidable_Region_State
{
    Active_Widget widget;

    Button_State divider_state;

    Slidable_Region_Theme *theme = NULL;

    Rect left_or_top_rect;
    Rect right_or_bottom_rect;
    Rect divider_rect;

    f32 divider_t = 0;

    f32 divider_over_factor = 0; // @Cleanup: We should move these into Button_State and get things from there, it should be more compact that way??
    f32 divider_pressed_factor = 0;

    bool dragging = false;
};

void dropdown_deactivate(Active_Widget *widget);

struct Dropdown_State
{
    Active_Widget widget;

    bool open = false;

    f32 open_t = 0.0f;

    Dropdown_State()
    {
        widget.deactivate_proc = dropdown_deactivate;
    }
};

enum Color_Pick_Mode : u32
{
    Hsl_Circle  = 0,
    Hsv_Sliders = 1,
    // Rgb_Sliders = 2,
    // Type_Ins    = 3,
    Count
};

struct Color_Picker_State
{
    Active_Widget widget;

    // For the circle slider.
    Vector2 ring_point = Vector2(1, 0);
    Vector2 disc_point = Vector2(1, 0);

    bool dragging_in_ring = false;
    bool dragging_in_disc = false;

    f32 dragging_in_ring_t = 0.0f;
    f32 dragging_in_disc_t = 0.0f;

    // For vertical slider.
    bool dragging_in_h = false;
    bool dragging_in_s = false;
    bool dragging_in_l = false;
    bool dragging_in_h_zoom = false;

    f32 dragging_in_h_t = 0.0f;
    f32 dragging_in_s_t = 0.0f;
    f32 dragging_in_l_t = 0.0f;
    f32 dragging_in_h_zoom_t = 0.0f;

    f32 zoom_hue_center = 0.0f; // In degrees; where the zoomed hue bar is centered. Doesn't move when you change the hue via that bar.

    Color_Pick_Mode color_pick_mode = Color_Pick_Mode::Hsl_Circle;

    Vector3 current_hsl_color;  // In 2.2 gamma space.
    Vector3 current_rgb_color;  // In 2.2 gamma space.

    Vector3 original_rgb_color; // In 2.2 gamma space.

    RArr<Vector3> stashed_colors;
};

struct Subwindow_State;

// @Cleanup: Collapse all these tables into one.
#include "table.h"
extern Table<u64, Button_State>            table_button;
extern Table<u64, Checkbox_State>          table_checkbox;
extern Table<u64, Text_Input_State>        table_text_input;
extern Table<u64, Slider_State>            table_slider;
extern Table<u64, Scrollable_Region_State> table_scrollable_region;
extern Table<u64, Dropdown_State>          table_dropdown;
extern Table<u64, Color_Picker_State>      table_color_picker;
extern Table<u64, Slidable_Region_State>   table_slidable_region;
extern Table<u64, Subwindow_State>         table_subwindow;

u64 combine_hashes(u64 a, u64 b);
u64 ui_get_hash(Source_Location loc, i64 identifier);

template <typename K, typename V>
V *find_or_create_state(Table<K, V> *table, K key, bool *created = NULL)
{
    auto widget_state = table_find_pointer(table, key);
    if (widget_state)
    {
        if (created) *created = false;

        start_using_state(&widget_state->widget);
        return widget_state;
    }

    V new_widget_state = {};
    widget_state = table_add(table, key, new_widget_state);
    widget_state->widget.widget_type = _make_Type(V);
    if (created) *created = true;

    start_using_state(&widget_state->widget);
    return widget_state;
}
