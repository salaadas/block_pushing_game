#pragma once

#include "common.h"

typedef void(*Popup_Proc)(void*);

void draw_popups();
void add_popup(Popup_Proc proc, void *data, i32 z);
void popups_per_frame_update();
