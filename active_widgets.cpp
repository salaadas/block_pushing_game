#include "active_widgets.h"

Active_Widget *ui_active_widget = NULL;
RArr<Active_Widget*> ui_state_stack; // These are all states we are currently drawing.

// void active_widgets_per_frame_update()
// {
// }

void active_widget_deactivate_all(Active_Widget *except_for_me)
{
    if (ui_active_widget && (ui_active_widget != except_for_me))
    {
        if (array_find(&ui_state_stack, ui_active_widget)) return; // This widget is what we are doing right now ... don't reset it!

        // Deactivate the current widget if it is not the excluded one.
        {
            auto proc = ui_active_widget->deactivate_proc;
            if (proc) proc(ui_active_widget);
        }

        ui_active_widget = NULL;
    }
}

void active_widget_add(Active_Widget *widget)
{
    if (ui_active_widget == widget) return;

    active_widget_deactivate_all(widget);
    ui_active_widget = widget;
}

void active_widget_remove(Active_Widget *widget)
{
    if (widget == ui_active_widget) ui_active_widget = NULL;
}

void start_using_state(Active_Widget *widget)
{
    array_add(&ui_state_stack, widget);
}

void stop_using_state(Active_Widget *widget)
{
    assert(ui_state_stack.count); // Assert that we have something in the stack before popping.
    auto old = pop(&ui_state_stack);
    assert((old == widget)); // Assert that what we popped is what we requested to stop using.
}
