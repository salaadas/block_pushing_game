/*
  With fonts, the approach is to load the dynamic fonts as a layer to get the glyphs.
  When we first call get_font_at_size(), we will get back a dynamic font and that dynamic font will be
  stored in an array of other dynamic fonts.

  The glyphs are not front-loaded all at once when we first call get_font_at_size(); however, it is lazily loaded
  when we use the font to render text. Then, it prepare the text, and find the glyph in the glyph_lookup table.
  If it doesn't find anything, it finally creates a glyph for the corresponding character. For some characters
  like tabs and newlines, we substitute in different visuals. If the requested character does not exist in the
  font file (.ttf, .otf, ...), we would substitute it with an unknown character, which is determined when we
  call get_font_at_size().
 */

#pragma once

#include "common.h"

// @Important: Load the <ft2build.h> before loading other freetype header files
#include <ft2build.h>
#include <freetype/freetype.h>

#include "table.h"
#include "texture.h"

struct Font_Line;

struct Font_Page
{
    Texture_Map      texture;
    Bitmap          *bitmap_data;  // @Cleanup: Not a pointer.
    i16              line_cursor_y;
    RArr<Font_Line*> lines;
    bool             dirty = false;
};

struct Font_Line
{
    Font_Page *page;
    i16        bitmap_cursor_x;
    i16        bitmap_cursor_y;
    i32        height;
};

struct Glyph_Data
{
    u32 utf32;
    u32 glyph_index_within_font;

    i16 x0, y0;
    u32 width, height;

    i16 offset_x, offset_y;

    i16 ascent; // Mainly for descent, actually
    i16 advance;

    Font_Page *page;
};

struct Font_Quad
{
    Vector2 p0, p1, p2, p3;
    f32     u0, v0, u1, v1;
    Glyph_Data *glyph;
};

struct Dynamic_Font
{
    String                   name;
    Table<i64, Glyph_Data*>  glyph_lookup;
    FT_Face                  face;

    i32                      character_height = 0;
    i32                      default_line_spacing; // amount in pixels
    i32                      max_ascender;
    i32                      max_descender;
    i32                      typical_ascender;
    i32                      typical_descender;
    i64                      em_width;
    i32                      x_advance;
    i32                      y_offset_for_centering;
    // Whether the most recent call to convert_to_temporary_glyphs() found a character it didn't know.
    bool                     glyph_conversion_failed = false;
    u32                      glyph_index_for_unknown_character = 119; // using 'w' for unknowns
    RArr<Glyph_Data*>        temporary_glyphs;
    // @Important:
    // temporary_glyphs.allocator = temp; // Using the temporary allocator for temporary_glyphs
    RArr<Font_Quad>          current_quads;
};

Dynamic_Font *get_font_at_size(String font_folder, String name, i32 pixel_height);
i64 prepare_text(Dynamic_Font *font, String text);
i64 get_text_width(Dynamic_Font *font, String s);
void generate_quads_for_prepared_text(Dynamic_Font *font, i64 x, i64 y);

void deinit_all_font_stuff_on_resize();

i32 utf8_char_length(u8 utf8_char);
u32 character_utf8_to_utf32(u8 *s, i64 source_length);
u8 *unicode_next_character(u8 *utf8);
