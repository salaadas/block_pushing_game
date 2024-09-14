#pragma once

#include "common.h"

#include "source_location.h"

struct Rect;
struct Color_Picker_Theme;

bool /*applied*/ color_picker(Rect r, Vector3 *input_and_output_color_rgb, Color_Picker_Theme *theme = NULL, i64 identifier = 0, Source_Location loc = Source_Location::current());
