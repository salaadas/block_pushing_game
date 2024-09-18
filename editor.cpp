#include "editor.h"
#include "sokoban.h"
#include "ui.h"

void editor_handle_event(Event *event)
{
    if (event->type == EVENT_TEXT_INPUT)
    {
        switch (event->utf32)
        {
            case '[': advance_level(-1); break;
            case ']': advance_level(+1); break;
        }
    }
}

void draw_editor()
{
    draw_ui(); //  :IncompleteUI
}
