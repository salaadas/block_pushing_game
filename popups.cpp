#include "popups.h"

struct Popup_Info
{
    Popup_Proc  proc = NULL;
    void       *data = NULL;
    i32 z      = 0;
    i32 serial = 0;
};

RArr<Popup_Info> popups;

inline
bool popups_sort(Popup_Info a, Popup_Info b)
{
    if (a.z == b.z) return a.serial - b.serial;
    else            return a.z - b.z;
}

void draw_popups()
{
    array_qsort(&popups, popups_sort);

    for (auto &it : popups)
    {
        assert(it.proc);
        it.proc(it.data);
    }
}

void add_popup(Popup_Proc proc, void *data, i32 z)
{
    Popup_Info info;
    info.proc   = proc;
    info.data   = data;
    info.z      = z;
    info.serial = static_cast<i32>(popups.count);

    array_add(&popups, info);
}

void popups_per_frame_update()
{
    popups.count = 0;
}
