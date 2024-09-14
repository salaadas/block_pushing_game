#pragma once

#include "common.h"

enum Key_Code : u32;
struct Rect;

bool ui_can_see_event(Key_Code key, void *handle, f32 z = FLT_MAX);
i32 /*old z*/ ui_set_z(i32 z);
void occlusion_per_frame_update();
void occlusion_declare(Rect rect, i32 z = 0);
