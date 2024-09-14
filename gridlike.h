#pragma once

#include "common.h"

struct Gridlike
{
    i64 squares_x, squares_y;
    i64 num_squares; // just squares_x * squares_y
    bool wrapping; //  = false;
};

void init_gridlike(Gridlike *gridlike);
my_pair<i64, i64> position_to_coordinates(Vector2 pos);
my_pair<i64, i64> position_to_coordinates(Vector3 pos);
i64 position_to_index(Gridlike g, Vector3 pos);
my_pair<i64, i64> get_ij(Gridlike g, i64 index);
void wrap_x(Gridlike g, i64 *x);
void wrap_y(Gridlike g, i64 *y);
my_pair<i64, i64> index_to_coordinates(Gridlike g, i64 index);
Vector3 get_position(Gridlike g, i64 index);
