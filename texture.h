#pragma once

#include "common.h"

#include <glad/glad.h>

enum class Texture_Format
{
    ARGB8888         = 0,
    RGB888           = 1,
    ARGBhalf         = 2,
    ARGB8888_NO_SRGB = 3,
    R8
};

struct Bitmap
{
    i32               width;
    i32               height;
    u8               *data;
    Texture_Format    format;
    i32               num_mipmap_levels;
    i64               length_in_bytes;
};

// We should move this into opengl
struct Texture_Map
{
    // @Note: For Catalog
    String           name;
    String           full_path;

    // We store additional fields of width and height because we
    // may use the Texture_Map as a framebuffer object
    i32              width;
    i32              height;
    bool             dirty = false;

    GLuint           id = 0;     // GL texture handle
    GLuint           fbo_id = 0; // GL texture handle

    Bitmap          *data = NULL; // Used if this is a texture we dirty

    // @Note: For Catalog
    bool             loaded = false;
};

void init_bitmap(Bitmap *bitmap);
void init_texture_map(Texture_Map *map);
