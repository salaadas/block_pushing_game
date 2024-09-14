#pragma once

#include "common.h"

#include "source_location.h"

struct Selection
{
    bool active = false;

    // These are all indices:
    i32 start_line      = -1;
    i32 start_character = 0;

    i32 end_line      = -1;
    i32 end_character = 0;
};

//
// In order to add auto-complete to a Text_Inpu,t implement a function with the following signature and call
// set_auto_complete(). The 'longest_match_length' return value is used by the Text_Input to color the text
// to show how much of what you typed is correct; thus it must represent the longest substring match even if
// that match is not complete (and thus 'match_results') is empty.
//
typedef i32/*longest match length?*/(*Auto_Complete)(String str, void *data, RArr<String> *match_results);

struct Text_Input_State;

struct Text_Input
{
    String text;

    bool entered       = false;
    bool escaped       = false;
    bool shift_plus_enter_was_pressed = false;

    Text_Input_State *text_input_state = NULL;

    static constexpr i64 MAX_BUFFER_SIZE = 8000;
    SArr<u8> input_buffer = NewArray<u8>(MAX_BUFFER_SIZE);

    RArr<String> command_history;

    bool initted = false;
    bool active  = false;

    f32 last_keypress_time = 0.0f;

    i32 insert_point = 0; // @Note: This is a i32 for now because the max capacity is 8000.
    i32 command_history_index = 0;

    String last_saved_input;
    bool cursor_tracking_mouse = false;

    // @Temporary: Move this to somewhere else
    bool did_initial_mouse_click_so_check_for_deadzone_change = false;

    bool do_active_widget_add            = false; // @Hack: while we figure out how to structure stuff.
    bool do_active_widget_deactivate_all = false; // @Hack: while we figure out how to structure stuff.

    Selection selection;

    // Auto complete stuff below:
    Auto_Complete auto_complete  = NULL;
    void *auto_complete_data     = NULL;
    bool  tab_pressed            = false;
    f32   completion_change_time = 0.0f;

    RArr<String> match_array;
    i32 match_length    = 0;
    i32 match_selection = 0;
    i32 longest_match   = -1;
};

void init(Text_Input *input);
void set_auto_complete(Text_Input *input, Auto_Complete proc, void *auto_complete_data);

void activate(Text_Input *input);
void deactivate(Text_Input *input);

void reset(Text_Input *input);

void set_text(Text_Input *input, String to_set); // This does not make the insert_point jumps to the end.
void add_text(Text_Input *input, String to_add);

struct Event;
struct Rect;
void handle_event(Text_Input *input, Event event);

void using_state_to_check_for_activating_text_input(Text_Input_State *text_input_state, Event event);
void check_for_activating_event(Text_Input *input, Event event);

struct Text_Input_Theme;
void draw(Text_Input *input, Rect r, Text_Input_Theme *theme = NULL, i64 identifier = 0, Source_Location loc = Source_Location::current());

struct Active_Widget;
void this_deactivate_is_not_the_same_as_the_normal_text_input_deactivate(Active_Widget *widget);
void this_handle_event_is_not_the_same_as_the_normal_text_input_handle_event(Active_Widget *widget, Event event);
