#include "ui_occlusion.h"

#include "ui.h"

struct Occlusion_Record
{
    i32  z = 0;
    i32  serial = 0; // Used in occlusion_per_frame_update to tie-break for equal z; the person who draws later goes on top.
    Rect rect;
};

i32 current_draw_z = 0;
RArr<Occlusion_Record> occlusions_this_frame;
RArr<Occlusion_Record> occlusions_next_frame;

bool ui_can_see_event(Key_Code key, void *handle, f32 z)
{
    if (z == FLT_MAX) z = current_draw_z;

    // @Robustness: We currently presume that the events happened at the current cursor position,
    // which may not be a safe assumption. We will want to revisit this!
    auto x = mouse_x_float;
    auto y = mouse_y_float;

    for (auto &it : occlusions_this_frame)
    {
        if (it.z <= z) return true;
        if (is_inside(x, y, it.rect)) return false;
    }

    return true;
}

i32 /*old z*/ ui_set_z(i32 z)
{
    auto old_z = current_draw_z;

    current_draw_z = z;

    return old_z;
}

inline
bool occlusions_sort(Occlusion_Record x, Occlusion_Record y)
{
    if (x.z == y.z)
    {
        return y.serial - x.serial;
    }
    else
    {
        return y.z - x.z;
    }
}

void occlusion_per_frame_update()
{
    swap_elements(&occlusions_this_frame, &occlusions_next_frame);
    occlusions_next_frame.count = 0;

    current_draw_z = 0;

    array_qsort(&occlusions_this_frame, occlusions_sort);

    // mouse_z = 0;
    auto x = mouse_x_float;
    auto y = mouse_y_float;

    for (auto &it : occlusions_this_frame)
    {
        if (is_inside(x, y, it.rect))
        {
            // mouse_z = it.z;
            // @Incomplete:
            break;
        }
    }
}

void occlusion_declare(Rect rect, i32 z)
{
    Occlusion_Record record;
    record.z      = z;
    record.serial = static_cast<i32>(occlusions_next_frame.count);
    record.rect   = rect;

    array_add(&occlusions_next_frame, record);
}
