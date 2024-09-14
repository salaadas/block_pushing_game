#pragma once

#include "common.h"

struct Event;

struct Active_Widget
{
    _type_Type widget_type = _make_Type(Active_Widget);

    // u32 last_ui_frame = 0;
    // bool is_on_state_stack = false;

    typedef void(*Event_Proc)(Active_Widget *w, Event event);
    typedef void(*Deactivate_Proc)(Active_Widget *w);

    Event_Proc      event_proc = NULL; // Maybe we would like to remove the need for this.
    Deactivate_Proc deactivate_proc = NULL;
};

extern Active_Widget *ui_active_widget;
extern RArr<Active_Widget*> ui_state_stack; // These are all states we are currently drawing.

void start_using_state(Active_Widget *widget);
void stop_using_state(Active_Widget *widget);

void active_widget_deactivate_all(Active_Widget *except_for_me = NULL);

void active_widget_add(Active_Widget *widget);
void active_widget_remove(Active_Widget *widget);

template <typename T>
T *CastDown_Widgets(Active_Widget *widget) // nocheckin, not tested.
{
    assert(cmp_var_type_to_type(widget->widget_type, T));

    T *result = reinterpret_cast<T*>(widget); // Since all widgets have an Active_Widget field as the first field and that is not heap allocated separately, we can do this trick to get from the pointer to the Active_Widget to the pointer of the derived widget.

    return result; // Returns the derived casted widget state.
}

