#pragma once

#include "common.h"
#include <glad/glad.h>

enum class Texture_Format
{
    UNINITIALIZE = 0,          // Framebuffers and shadow map buffers and stuff like that have the format set to UNINITIALIZED because really they should not be used in update_texture() and such.
    ARGB8888 = 1,              // Fixed point formats.
    RGB888,
    ARGBhalf,
    R8,
    RG88,

    DXT1,                      // Block compression format.
    DXT3,
    DXT5,
};

struct Bitmap
{
    i32               width  = 0;
    i32               height = 0;
    // i64               length_in_bytes = 0; // Probably will need this for stb_image_write().

    u8                *data  = NULL;

    Texture_Format    XXX_format = Texture_Format::UNINITIALIZE; // @Cleanup: Reality is that we only need this for font.cpp, the rest of the stuff don't even need Bitmap after loading the file...
    i32               XXX_num_mipmap_levels = 1; // @Cleanup: Same as above.
};

// We should move this into opengl
struct Texture_Map
{
    // @Note: For Catalog
    String           name;
    String           full_path;

    i32              width  = 0;
    i32              height = 0;
    bool             dirty  = false; // @Cleanup: Remove me.

    bool             is_srgb     = false;
    bool             is_embedded = false; // If texture is an embedded texture of the model, we cannot reload it...

    GLuint           id     = 0; // GL texture handle
    GLuint           fbo_id = 0; // GL texture handle

    Texture_Format   format = Texture_Format::UNINITIALIZE;
    i32              num_mipmap_levels = 1;

    // @Note: For Catalog
    bool             loaded = false;

};

struct Opengl_Texture_Info
{
    GLenum dest_format  = GL_INVALID_ENUM;
    GLenum src_format   = GL_INVALID_ENUM;
    GLenum src_type     = GL_INVALID_ENUM;
    i32    alignment    = 4;
    bool   compressed   = false;
    i32    block_size   = 0; // Only applies for compressed textures.
};

Opengl_Texture_Info get_ogl_format(Texture_Format tf, bool srgb);
Texture_Format texture_format_with_this_nchannels(i32 channels_count);

void init_bitmap(Bitmap *bitmap);
void init_texture_map(Texture_Map *map);
