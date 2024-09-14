#include "texture.h"

void init_bitmap(Bitmap *bitmap)
{
    bitmap->width  = 0;
    bitmap->height = 0;
    bitmap->data   = NULL;
    bitmap->num_mipmap_levels = 1;
    bitmap->length_in_bytes = 0;
}

void init_texture_map(Texture_Map *map)
{
    // init_string(&map->name);
    // init_string(&map->full_path);
    map->id     = 0xffffffff;
    map->fbo_id = 0; // 0xffffffff;
    map->width  = 0;
    map->height = 0;
    map->data   = NULL;
    map->dirty  = false;
    map->loaded = false;
}
