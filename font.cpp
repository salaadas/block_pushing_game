#include "font.h"

#include <cwchar>

FT_Library ft_library;
bool fonts_initted = false;

// We probably should make different page sizes
// for different fonts.
i32 the_page_size_x = 2048;
i32 the_page_size_y = 1024;

RArr<Dynamic_Font*> dynamic_fonts;
RArr<Font_Page*>    font_pages;

#include "pool.h"
Pool glyph_and_line_pool;

void init_font_page(Font_Page *memory)
{
    init_texture_map(&memory->texture);
    memory->bitmap_data = NULL;

    memory->dirty = false;

    array_init(&memory->lines);
}

void init_dynamic_font(Dynamic_Font *font)
{
    font->face = NULL;

    font->glyph_conversion_failed           = false;
    font->glyph_index_for_unknown_character = 119; // using 'w' for unknowns

    array_init(&font->temporary_glyphs); // @Fixme: Inspect why we cannot use temporary storage for this.
    // font->temporary_glyphs.allocator = {global_context.temporary_storage, __temporary_allocator};

    array_init(&font->current_quads);

    default_table(&font->glyph_lookup); // @Cleanup: Isn't this redundant?
}

//
// @Cleanup: Unicode stuff.
//

i32 utf8_char_length(u8 utf8_char)
{
    if (utf8_char < 0x80)
    {
        // printf("0xxxxxxx\n");
        return 1;
    }
    else if ((utf8_char & 0xe0) == 0xc0)
    {
        // printf("110xxxxx");
        return 2;
    }
    else if ((utf8_char & 0xf0) == 0xe0)
    {
        // printf("1110xxxx");
        return 3;
    }
    else if (((utf8_char & 0xf8) == 0xf0) && (utf8_char < 0xf4)) // @Check: does utf8 even has an upper bound other than 0xff?
    {
        // printf("11110xxx");
        return 4;
    }

    printf("Invalid utf8 char with value %x\n", utf8_char);
    return 0;
}

i32 utf8_valid(u8 *utf8)
{
    i32 length = utf8_char_length(*utf8);

    // Check trailing bytes and invalid utf8
    switch (length)
    {
        case 4: if ((utf8[3] & 0xc0) != 0x80) return 0;
        case 3: if ((utf8[2] & 0xc0) != 0x80) return 0;
        case 2: if ((utf8[1] & 0xc0) != 0x80) return 0;
        case 1: return length; // No trailing bytes to validate
        case 0: return 0; // Invalid
    }

    return length;
}

u8 *unicode_next_character(u8 *utf8)
{
    auto t = utf8;
    auto length_of_current_char = utf8_char_length(*utf8);
    auto result = t + length_of_current_char;

    return result;
}

u32 character_utf8_to_utf32(u8 *s, i64 source_length)
{
    auto length = utf8_valid(s);

    if (length > source_length)
    {
        printf("Not enough length for that utf32 character, so it is invalid\n");
        return 0;
    }

    switch (length)
    {
        case 0: return 0; // Invalid
        case 1: return *s; // No work required
        case 2: return ((s[0] & 0x1f) <<  6) |  (s[1] & 0x3f);
        case 3: return ((s[0] & 0x0f) << 12) | ((s[1] & 0x3f) <<  6) |  (s[2] & 0x3f);
        case 4: return ((s[0] & 0x07) << 18) | ((s[1] & 0x3f) << 12) | ((s[2] & 0x3f) << 6) | (s[3] & 0x3f);
    }

    return 0;
}

bool my_utf_cmp(i64 a, i64 b) {return a == b;}
u32  my_utf_hash(i64 x) {return x;}

//
// End of Unicode stuff.
//

inline
i64 FT_ROUND(i64 x)
{
    if (x >= 0) return (x + 0x1f) >> 6;
    return -(((-x) + 0x1f) >> 6);
}

template <typename V>
void temporary_array_reset(RArr<V> *array)
{
    array_reset(array);
    // array->count     = 0;
    // array->allocated = 0;
}

i32 get_image_bytes_per_texel(Texture_Format format)
{
    // @Incomplete:
    if (format == Texture_Format::R8)
    {
        return 1;
    }

    printf("******************* Texture format is undeclared\n");
    assert(0);
}

void bitmap_alloc(Bitmap *bitmap, i32 w, i32 h, Texture_Format format)
{
    bitmap->width  = w;
    bitmap->height = h;

    auto bpp  = get_image_bytes_per_texel(format);
    auto size = w * h * bpp;

    bitmap->data              = (u8*)(my_alloc(sizeof(u8) * size));
    // bitmap->length_in_bytes   = size;
    bitmap->XXX_num_mipmap_levels = 1;
    bitmap->XXX_format            = format;
}

void init_fonts(i32 page_size_x = -1, i32 page_size_y = -1)
{
    // We don't want to init the freetype library twice...
    assert((!fonts_initted));

    if ((page_size_x >= 0) || (page_size_y >= 0))
    {
        assert((page_size_x >= 64));
        assert((page_size_y >= 64));
        the_page_size_x = page_size_x;
        the_page_size_y = page_size_y;
    }

    fonts_initted = true;

    auto error = FT_Init_FreeType(&ft_library);
    assert((!error));

    set_allocators(&glyph_and_line_pool);
    glyph_and_line_pool.memblock_size = 100 * sizeof(Glyph_Data);
}

bool is_latin(u32 utf32)
{
    // 0x24f is the end of Latin Extended-B
    if (utf32 > 0x24f)
    {
        if ((utf32 >= 0x2000) && (utf32 <= 0x218f))
        {
            // General punctuation, currency symbols, number forms, etc.
        }
        else return false;
    }

    return true;
}

inline
void ensure_fonts_are_initted()
{
    if (!fonts_initted) init_fonts();
}

Font_Line *find_line_within_page(Font_Page *page, i64 width, i64 height)
{
    auto bitmap = page->bitmap_data;

    for (auto it : page->lines)
    {
        if (it->height < height) continue; // Line too short!
        if (((it->height * 7) / 10) > height) continue; // Line too tall!

        if ((bitmap->width - it->bitmap_cursor_x) < width) continue; // No room at end of line!

        return it; // Found one!
    }

    // If there's not enough room to start a new line, return
    auto height_remaining = bitmap->height - page->line_cursor_y;

    if (height_remaining < height) return NULL;

    // Or if for some reason the page is too narrow for the character...
    // In this case, starting a new line would not help!
    if (bitmap->width < width) return NULL;

    // Start a new line... with some extra space for expansion if we have room.
    auto desired_height = (height * 11) / 10;

    if (desired_height > height_remaining) desired_height = height_remaining;

    Font_Line *line = (Font_Line*)get(&glyph_and_line_pool, sizeof(Font_Line));

    if (!line) return NULL;

    line->page            = page;
    line->bitmap_cursor_x = 0;
    line->bitmap_cursor_y = page->line_cursor_y;
    line->height          = desired_height;

    array_add(&page->lines, line);
    // printf("******************* page->lines = %ld\n", page->lines.count);

    page->line_cursor_y += (i16)desired_height;

    return line;
}

Font_Page *make_font_page()
{
    auto page = New<Font_Page>(false);
    init_font_page(page);
    // printf("******************* Making new font page\n");

    auto bitmap = New<Bitmap>(false);
    bitmap_alloc(bitmap, the_page_size_x, the_page_size_y, Texture_Format::R8);
    // printf("******************* Allocing bitmap for the font page\n");
    page->bitmap_data = bitmap;

    array_add(&font_pages, page);
    // printf("******************* Added page to the list of font pages\n");

    return page;
}

Font_Line *get_font_line(i64 width, i64 height)
{
    for (auto page : font_pages)
    {
        auto line = find_line_within_page(page, width, height);
        if (line) return line;
    }

    auto page = make_font_page();
    auto line = find_line_within_page(page, width, height); // If we didn't find it somehow, we lose!!
    if (line == NULL)
    {
        fprintf(stderr, "Couldn't find a line for a character in a fresh font page. This is a bug.\n");
        assert(0);
        return NULL;
    }

    return line;
}

void copy_glyph_to_bitmap(FT_Face face, Glyph_Data *data)
{
    auto b = &face->glyph->bitmap;
    // printf("Copying glyph index %d to bitmap\n", data->glyph_index_within_font);

    data->width    = b->width;
    data->height   = b->rows;
    data->advance  = (i16)(face->glyph->advance.x >> 6);
    data->offset_x = (i16)(face->glyph->bitmap_left);
    data->offset_y = (i16)(face->glyph->bitmap_top);

    auto metrics = &face->glyph->metrics;
    // This truncation seemed necessary because at least one font gave Jon blow weird data.
    // Maybe it's a buggy font, or maybe he was doing something weird/dumb.
    data->ascent = (i16)(metrics->horiBearingY >> 6);

    auto font_line = get_font_line(b->width, b->rows);

    auto dest_x = font_line->bitmap_cursor_x;
    auto dest_y = font_line->bitmap_cursor_y;

    data->x0   = dest_x;
    data->y0   = dest_y;
    data->page = font_line->page;

    auto bitmap = font_line->page->bitmap_data;

    auto rows  = (i32)(b->rows); // Freetype changes the rows and width types to unsigned, and they may be zero
    auto width = (i32)(b->width);
    for (i32 j = 0; j < rows; ++j)
    {
        for (i32 i = 0; i < width; ++i)
        {
            auto dest_pixel = bitmap->data + ((dest_y + j) * bitmap->width + (dest_x + i));
            *dest_pixel = b->buffer[(rows - 1 - j) * b->pitch + i];
        }
    }

    font_line->bitmap_cursor_x += (i16)b->width;
    font_line->page->dirty = true;
}

Glyph_Data *find_or_create_glyph(Dynamic_Font *font, u32 utf32)
{
    i64 hash_key = utf32;

    // printf("Finding glyph for char %c\n", (char)utf32);
    auto [data, success] = table_find(&font->glyph_lookup, hash_key);
    if (success)
    {
        // printf("Found %c with glyph index %u\n", (char)utf32, data->glyph_index_within_font);
        return data;
    }

    if (utf32 == '\t') utf32 = L'→'; // draw tabs as arrows
    if (utf32 == '\n') utf32 = L'¶';  // draw newlines as paragraph symbols

    auto glyph_index = FT_Get_Char_Index(font->face, utf32);
    if (!glyph_index)
    {
        wprintf(L"Unable to find a glyph in font '%s' for utf32 character '%lc', its value is '%d'\n",
                temp_c_string(font->name), utf32, utf32);
        glyph_index = font->glyph_index_for_unknown_character;
    }
    auto error = FT_Load_Glyph(font->face, glyph_index, FT_LOAD_DEFAULT);
    assert((!error));
    FT_Render_Glyph(font->face->glyph, FT_RENDER_MODE_LCD);

    data = (Glyph_Data*)(get(&glyph_and_line_pool, sizeof(Glyph_Data)));

    data->utf32 = utf32;
    data->glyph_index_within_font = glyph_index;

    copy_glyph_to_bitmap(font->face, data);

    // printf("Added char %c code %d to the glyph table, with index %u\n", (char)utf32, utf32, data->glyph_index_within_font);
    table_add(&font->glyph_lookup, hash_key, data);
    // printf("******************* Added glyph with hash key %ld to glyph_lookup table of font '%s'\n",
    //        hash_key, temp_c_string(font->name));

    return data;
}

i64 convert_to_temporary_glyphs(Dynamic_Font *font, String s)
{
    font->glyph_conversion_failed = false;
    temporary_array_reset(&font->temporary_glyphs);

    if (!s) return 0;

    auto use_kerning = FT_HAS_KERNING(font->face);
    u32  prev_glyph = 0;

    auto width_in_pixels = 0;

    auto t = s.data;

    while (t < (s.data + s.count))
    {
        auto utf32 = character_utf8_to_utf32(t, s.data + s.count - t);
        auto glyph = find_or_create_glyph(font, utf32);

        if (glyph)
        {
            array_add(&font->temporary_glyphs, glyph);

            width_in_pixels += glyph->advance;

            if (use_kerning && prev_glyph)
            {
                FT_Vector delta;
                auto error = FT_Get_Kerning(font->face, prev_glyph, glyph->glyph_index_within_font,
                                            FT_KERNING_DEFAULT, &delta);
                if (!error) width_in_pixels += (delta.x >> 6);
            }

            // FreeType returns glyph index 0 for undefined glyphs.. just signal
            // the condition that this happened.
            if (glyph->glyph_index_within_font == 0) font->glyph_conversion_failed = true;

            prev_glyph = glyph->glyph_index_within_font;
        }

        t = unicode_next_character(t);
    }

    return width_in_pixels;
}

bool set_unknown_character(Dynamic_Font *font, u32 utf32)
{
    auto index = FT_Get_Char_Index(font->face, utf32);
    if (!index) return false;

    font->glyph_index_for_unknown_character = index;
    return true;
}

// This gets called in get_font_at_size(...)
void load_font_part_2(Dynamic_Font *result, i32 pixel_height)
{
    auto face = result->face;

    auto success = FT_Set_Pixel_Sizes(face, 0, pixel_height);

    result->face = face;
    result->character_height = pixel_height;

    auto y_scale_font_to_pixels  = face->size->metrics.y_scale / (64.0f * 65536.0f);
    result->default_line_spacing = (i32)(floorf(y_scale_font_to_pixels * face->height + 0.5f));
    result->max_ascender         = (i32)(floorf(y_scale_font_to_pixels * face->bbox.yMax + 0.5f));
    result->max_descender        = (i32)(floorf(y_scale_font_to_pixels * face->bbox.yMin + 0.5f));

    // We intentionally don't use the max ascender, because
    // it doesn't tend to look right. So we use 'm'... but for
    // Chinese, for example, this is going to be wrong, so maybe
    // this is @Incomplete and we need to have multiple options.

    auto glyph_index = FT_Get_Char_Index(face, 'm');
    if (glyph_index)
    {
        FT_Load_Glyph(face, glyph_index, FT_LOAD_DEFAULT);
        result->y_offset_for_centering = (i32)(0.5f * FT_ROUND(face->glyph->metrics.horiBearingY) + 0.5f);
    }

    glyph_index = FT_Get_Char_Index(face, 'M');
    if (glyph_index)
    {
        FT_Load_Glyph(face, glyph_index, FT_LOAD_DEFAULT);
        result->em_width  = FT_ROUND(face->glyph->metrics.width);
        result->x_advance = FT_ROUND(face->glyph->metrics.horiAdvance);
    }

    glyph_index = FT_Get_Char_Index(face, 'T');
    if (glyph_index)
    {
        FT_Load_Glyph(face, glyph_index, FT_LOAD_DEFAULT);
        result->typical_ascender = FT_ROUND(face->glyph->metrics.horiBearingY);
    }

    glyph_index = FT_Get_Char_Index(face, 'g');
    if (glyph_index)
    {
        FT_Load_Glyph(face, glyph_index, FT_LOAD_DEFAULT);
        result->typical_descender = FT_ROUND(face->glyph->metrics.horiBearingY - face->glyph->metrics.height);
    }

    auto error = FT_Select_Charmap(face, FT_ENCODING_UNICODE);
    if (error)
    {
        fprintf(stderr, "Couldn't select unicode charmap for font '%s'\n", temp_c_string(result->name));
        assert(0);
    }

    {
        // 0xfffd is Unicode's replacement character
        auto success = set_unknown_character(result, 0xfffd);
        if (!success) success = set_unknown_character(result, 0x2022); // Bullet character
        if (!success) success = set_unknown_character(result, '?');
        if (!success)
        {
            fprintf(stderr, "Unable to set unknown character for font '%s'\n", temp_c_string(result->name));
        }
    }

    array_add(&dynamic_fonts, result);
    // printf("******************* Added new font '%s' with height %d to list of dyn_fonts (%ld)\n",
    //        temp_c_string(result->name), pixel_height, dynamic_fonts.count);
}

#include "file_utils.h"

Dynamic_Font *get_font_at_size(String font_folder, String name, i32 pixel_height)
{
    ensure_fonts_are_initted();

    // Try to find a previously loaded font
    for (auto it : dynamic_fonts)
    {
        if (it->character_height != pixel_height) continue;
        if (!equal(it->name, name))               continue;
        return it;
    }

    // @Temporary: Should we read entire font file every time we load a font?
    // ^ This is even for loading a different size with a same font.
    // Maybe cache the font binary file somewhere for later use if this is a bottle-neck.
    if (font_folder[font_folder.count - 1] == '/') font_folder.count -= 1;
    String full_path = tprint(String("%s/%s"), temp_c_string(font_folder), name.data);

    // Create a new font face for Dynamic_Font rather than sharing one between fonts.
    // The reason is because we don't want to keep changing the size every time we want to
    // do anything and worry whether another Dynamic_Font has changed the size.
    FT_Face face;
    auto error = FT_New_Face(ft_library, (char*)full_path.data, 0, &face);
    if (error)
    {
        // @Hardcode:
        fprintf(stderr, "              !! Error(%d) while loading font '%s/%s', reverting to default font: '%s'\n",
                error, temp_c_string(font_folder), temp_c_string(name), "KarminaBoldItalic");

        error = FT_New_Face(ft_library, "data/fonts/KarminaBoldItalic.otf", 0, &face);

        assert((!error));
    }

    auto result = New<Dynamic_Font>(false);
    init_dynamic_font(result);

    result->name = copy_string(name);
    result->face = face;
    load_font_part_2(result, pixel_height);

    // result->glyph_lookup.count           = 0;
    // result->glyph_lookup.allocated       = 0;
    // result->glyph_lookup.slots_filled    = 0;
    // result->glyph_lookup.add_collisions  = 0;
    // result->glyph_lookup.find_collisions = 0;
    // result->glyph_lookup.LOAD_FACTOR_PERCENT = 70;
    // result->glyph_lookup.allocator = (Allocator){};

    init(&result->glyph_lookup);

    result->glyph_lookup.cmp_function  = my_utf_cmp;
    result->glyph_lookup.hash_function = my_utf_hash;

    return result;
}

i64 prepare_text(Dynamic_Font *font, String text)
{
    auto width = convert_to_temporary_glyphs(font, text);
    return width;
}

// Like prepare_text, but it doesn't create any temporary glyphs.
// This is useful for finding the cursor position within the text
i64 get_text_width(Dynamic_Font *font, String s)
{
    if (!s) return 0;

    auto use_kerning = FT_HAS_KERNING(font->face);
    u32 prev_glyph   = 0;

    auto width_in_pixels = 0;

    auto t = s.data;
    while (t < (s.data + s.count))
    {
        auto utf32 = character_utf8_to_utf32(t, s.data + s.count - t);
        auto glyph = find_or_create_glyph(font, utf32);

        if (glyph)
        {
            width_in_pixels += glyph->advance;

            if (use_kerning && prev_glyph)
            {
                FT_Vector delta;
                auto error = FT_Get_Kerning(font->face, prev_glyph, glyph->glyph_index_within_font,
                                            FT_KERNING_DEFAULT, &delta);
                if (!error) width_in_pixels += (delta.x >> 6);
            }

            prev_glyph = glyph->glyph_index_within_font;
        }

        t = unicode_next_character(t);
    }

    return width_in_pixels;
}

i32 get_cursor_pos_for_width(Dynamic_Font *font, String s, i64 requested_width)
{
    if (!s) return 0;

    auto use_kerning = FT_HAS_KERNING(font->face);
    u32 prev_glyph = 0;

    i32 current_pos = 0;
    auto width_in_pixels = 0;

    auto t = s.data;
    while (t < (s.data + s.count))
    {
        auto utf32 = character_utf8_to_utf32(t, s.data + s.count - t);
        auto glyph = find_or_create_glyph(font, utf32);

        auto glyph_width = 0;

        if (glyph)
        {
            glyph_width += glyph->advance;

            if (use_kerning && prev_glyph)
            {
                FT_Vector delta;
                auto error = FT_Get_Kerning(font->face, prev_glyph, glyph->glyph_index_within_font,
                                            FT_KERNING_DEFAULT, &delta);
                if (!error) width_in_pixels += (delta.x >> 6);
            }
        }

        t = unicode_next_character(t);
        prev_glyph = glyph->glyph_index_within_font;

        width_in_pixels += glyph_width;
        if (width_in_pixels >= requested_width)
        {
            // If we're closer to the next glyph position, return the next position.
            if ((width_in_pixels - requested_width) <= (glyph_width / 2))
                current_pos = (i32)(t - s.data);
        }

        current_pos = (i32)(t - s.data);
    }

    return current_pos;
}

void generate_quads_for_prepared_text(Dynamic_Font *font, i64 x, i64 y)
{
    assert((font != NULL));

    array_reset(&font->current_quads);
    array_reserve(&font->current_quads, font->temporary_glyphs.count);

    // @Speed: we don't need kerning for most of the text
    auto use_kerning = FT_HAS_KERNING(font->face);

    auto sx = (f32)x;
    auto sy = (f32)y; // Not changing sy because we are rendering on the same line

    u32 prev_glyph = 0;

    // @Speed!
    for (auto info : font->temporary_glyphs)
    {
        if (!info->page) continue;

        if (use_kerning && prev_glyph)
        {
            FT_Vector delta;
            auto error = FT_Get_Kerning(font->face, prev_glyph, info->glyph_index_within_font,
                                        FT_KERNING_DEFAULT, &delta);

            if (!error)
            {
                sx += (f32)(delta.x >> 6);
            }
            else
            {
                fprintf(stderr, "Couldn't get kerning for glyphs %u, %u\n",
                        prev_glyph, info->glyph_index_within_font);
            }
        }

        auto sx1 = sx  + (f32)info->offset_x;
        auto sx2 = sx1 + (f32)info->width;

        auto sy2 = sy  + (f32)info->ascent;
        auto sy1 = sy2 - (f32)info->height;

        Font_Quad quad;
        quad.glyph = info;

        quad.p0.x = sx1;
        quad.p0.y = sy1;

        quad.p1.x = sx2;
        quad.p1.y = sy1;

        quad.p2.x = sx2;
        quad.p2.y = sy2;

        quad.p3.x = sx1;
        quad.p3.y = sy2;

        // We are not using the texture map's width, as the texture may be dirty.
        auto width  = info->page->bitmap_data->width;
        auto height = info->page->bitmap_data->height; // Ibid.

        quad.u0 = info->x0 / (f32)width;
        quad.u1 = ((f32)info->x0 + info->width) / width;
        quad.v0 = info->y0 / (f32)height;
        quad.v1 = ((f32)info->y0 + info->height) / height;

        array_add(&font->current_quads, quad);
        sx += (f32)info->advance;
        prev_glyph = info->glyph_index_within_font;
    }
}

void deinit_all_font_stuff_on_resize()
{
    if (!fonts_initted) return;
    fonts_initted = false;

    release(&glyph_and_line_pool);

    for (auto it : font_pages)
    {
        my_free(it->bitmap_data->data);
        my_free(it->bitmap_data);
        array_free(&it->lines);

        my_free(it);
    }
    array_free(&font_pages);
    // printf("Freed all the font pages and bitmaps\n");

    for (auto it : dynamic_fonts)
    {
        // printf("Freeing font '%s'\n", temp_c_string(it->name));
        FT_Done_Face(it->face);
        free_string(&it->name);
        deinit(&it->glyph_lookup);
        array_free(&it->current_quads);
        array_free(&it->temporary_glyphs);

        my_free(it);
    }
    array_free(&dynamic_fonts);
    // printf("Freed all the dynamic fonts\n");

    FT_Done_FreeType(ft_library);
}
