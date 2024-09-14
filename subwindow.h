#pragma once

#include "common.h"

#include "ui.h"

// struct Subwindow_Info
// {
//     bool open = false; // For open/close.
// };

struct Subwindow_State
{
    Active_Widget widget;

    i32 num_begins = 0; // What is this??? nocheckin
    u64 hash = 0; // Used in end_subwindow(), to combine the hash with the title bar hash.

    enum Dragging_Type : u8
    {
        NOTHING   = 0,
        TITLE     = 1,
        TO_RESIZE = 2,
    };

    Dragging_Type dragging = NOTHING;
    f32 dragging_last_x = 0;
    f32 dragging_last_y = 0;

    Rect title_bar_rect;
    Rect content_area; // This is the area excluding the title bar.
    Rect full_rect;    // This is the rect that you passed into begin_subwindow().

    Subwindow_Theme theme;
};

Subwindow_State *begin_subwindow(Rect r, Subwindow_Theme *theme = NULL, i64 identifier = 0, Source_Location loc = Source_Location::current());
Rect end_subwindow(Subwindow_State *state, String title_bar_string = String(""));
