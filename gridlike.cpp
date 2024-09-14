#include "gridlike.h"

void init_gridlike(Gridlike *gridlike)
{
    gridlike->squares_x   = 0;
    gridlike->squares_y   = 0;
    gridlike->num_squares = 0;
    gridlike->wrapping    = false;
}

my_pair<i64, i64> position_to_coordinates(Vector2 pos)
{
    return {static_cast<i64>(floor(pos.x + 0.1)),
            static_cast<i64>(floor(pos.y + 0.1))};
}

my_pair<i64, i64> position_to_coordinates(Vector3 pos)
{
    return {static_cast<i64>(floor(pos.x + 0.1)),
            static_cast<i64>(floor(pos.y + 0.1))};
}

i64 position_to_index(Gridlike g, Vector3 pos)
{
    auto [i, j] = position_to_coordinates(pos);

    if (i < 0) return -1;
    if (j < 0) return -1;

    if (i >= g.squares_x) return -1;
    if (j >= g.squares_y) return -1;

    return j * g.squares_x + i;
}

my_pair<i64, i64> get_ij(Gridlike g, i64 index)
{
    if (index == -1) return {0, 0}; // Should not happen

    return {(index % g.squares_x), static_cast<i64>(index / g.squares_x)};
}

void wrap_x(Gridlike g, i64 *x)
{
    if (*x < 0)            { *x += g.squares_x; }
    if (*x >= g.squares_x) { *x -= g.squares_x; }
}

void wrap_y(Gridlike g, i64 *y)
{
    if (*y < 0)            { *y += g.squares_y; }
    if (*y >= g.squares_y) { *y -= g.squares_y; }
}

void wrap(Gridlike g, i64 *x, i64 *y)
{
    wrap_x(g, x);
    wrap_y(g, y);
}

my_pair<i64, i64> index_to_coordinates(Gridlike g, i64 index)
{
    return {(index % g.squares_x), (index / g.squares_x)};
}

Vector3 get_position(Gridlike g, i64 index)
{
    auto i = index % g.squares_x;
    auto j = index / g.squares_y;

    return Vector3(static_cast<f32>(i), static_cast<f32>(j), 0);
}

