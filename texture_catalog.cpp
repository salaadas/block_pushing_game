#include "texture_catalog.h"

#include <stb_image.h>
#include "opengl.h"
#include "file_utils.h"

void init_texture_catalog(Texture_Catalog *catalog)
{
    //
    // @Note:
    // Set uv flip for stb here. In fact, our application assumes that the textures loaded are
    // the same as how they are viewed on the Desktop. EXCEPT for block compressed texture,
    // where I could not figure out how to flip the texture there, so the block compressed one
    // needs to be flipped before loading.
    //
    stbi_set_flip_vertically_on_load(true);

    catalog->base.my_name = String("Textures");
    array_add(&catalog->base.extensions, String("jpg"));
    array_add(&catalog->base.extensions, String("png"));
    array_add(&catalog->base.extensions, String("dds"));

    // array_add(&catalog->base.extensions, String("bmp"));
    // array_add(&catalog->base.extensions, String("tga"));

    do_polymorphic_catalog_init(catalog);
}

/*
void deinit_texture_catalog(Texture_Catalog *catalog) // @Cleanup: Remove this?
{
    array_free(&catalog->base.extensions);

    for (auto it : catalog->table)
    {
        free_string(&it.key);

        auto map = it.value;

        free_string(&map->name);
        free_string(&map->full_path);

        stbi_image_free(map->data->data);
        my_free(map->data);
        my_free(map);
    }

    deinit(&catalog->table);
}
*/

Texture_Map *make_placeholder(Texture_Catalog *catalog, String short_name, String full_path)
{
    Texture_Map *map = New<Texture_Map>();
    init_texture_map(map);
    map->name      = copy_string(short_name);
    map->full_path = copy_string(full_path);

    return map;
}

inline
void load_non_block_compressed_texture_helper(Texture_Map *map) // Load from file.
{
    i32 width, height;
    i32 components;

    auto c_path = temp_c_string(map->full_path);

    u8 *data = stbi_load((char*)c_path, &width, &height, &components, 0);
    if (!data)
    {
        logprint("texture_catalog", "stb_image failed to load bitmap from path '%s'\n", c_path);
        return;
    }

    Bitmap result;
    result.width  = width;
    result.height = height;
    result.data   = data;
    // result.length_in_bytes = width * height * components;

    map->format = texture_format_with_this_nchannels(components);

    map->num_mipmap_levels = 1; // @Hardcoded: How to know?
    update_texture_from_bitmap(map, &result);

    stbi_image_free(result.data);
}

inline
bool load_dds_texture_helper(Texture_Map *map)
{
    auto c_path = (char*)temp_c_string(map->full_path);
    auto [original_data, success] = read_entire_file(map->full_path);

    // printf("Loading dds texture from path '%s'!\n", c_path);

    if (!success)
    {
        logprint("load_dds_texture", "Unable to load DDS texture from path '%s'!\n", c_path);
        return false;
    }

    defer { free_string(&original_data); };

    // Verify the 'DDS ' in the actual file itself.
    constexpr auto SIGNATURE_SIZE = 4; // 4 bytes to store the 'DDS ' sequence.
    {
        auto t  = original_data;
        t.count = SIGNATURE_SIZE;

        if (t != String("DDS "))
        {
            logprint("load_dds_texture", "Even though the texture from path '%s' has extension .dds, it is actually not a DDS texture!\n", c_path);
            return false;
        }
    }

    auto read_u32_with_offset = [](String data, i32 offset) -> u32 {
        auto u8_address = &data.data[offset];
        auto value = *(reinterpret_cast<u32*>(u8_address));

        return value;
    };

    //
    // Read the image description from the header.
    //
    constexpr auto HEADER_SIZE = 124;
    auto data = original_data;

    auto height = read_u32_with_offset(data, 12);
    auto width  = read_u32_with_offset(data, 16);

    auto linear_size  = read_u32_with_offset(data, 20);
    auto mipmap_count = read_u32_with_offset(data, 28);
    auto fourcc       = read_u32_with_offset(data, 84);

    auto image_buffer = original_data;
    image_buffer.data += SIGNATURE_SIZE + HEADER_SIZE;
    image_buffer.count = (mipmap_count > 1) ? (linear_size * 2) : linear_size;

    // @Incomplete: Not handling DXT2 and DXT4.
    // @Incomplete: Not handling DXT2 and DXT4.
    constexpr u32 FOURCC_DXT1 = 0x31545844; // 'DXT1' in ascii.   aka BC1.
    constexpr u32 FOURCC_DXT3 = 0x33545844; // 'DXT3' in ascii.   aka BC3.
    constexpr u32 FOURCC_DXT5 = 0x35545844; // 'DXT5' in ascii.   aka BC5.
    assert((fourcc == FOURCC_DXT1) || (fourcc == FOURCC_DXT3) || (fourcc == FOURCC_DXT5));

    auto num_components = (fourcc == FOURCC_DXT1) ? 3 : 4; // Only BC1 has RGB. (I think...)
    GLenum gl_format;

    Texture_Format texture_format;
    switch (fourcc)
    {
        case FOURCC_DXT1:  texture_format = Texture_Format::DXT1; break;
        case FOURCC_DXT3:  texture_format = Texture_Format::DXT3; break;
        case FOURCC_DXT5:  texture_format = Texture_Format::DXT5; break;
        default: {
            logprint("load_dds_texture", "Fourcc of the texture '%s' is not of the known type (%X)....\n", c_path, fourcc);
            return false;
        };
    }

    Bitmap bitmap;
    bitmap.width  = width;
    bitmap.height = height;
    bitmap.data   = image_buffer.data;

    map->format            = texture_format;
    map->num_mipmap_levels = mipmap_count;

    update_texture_from_bitmap(map, &bitmap);
    return true;
}

//
// The file is assumed to be map->full_path.
//
void load_texture_from_file(Texture_Map *map, bool is_srgb)
{
    auto extension = find_character_from_right(map->full_path, '.');
    advance(&extension, 1); // Skip the '.'

    if (map->is_embedded)
    {
        logprint("load_texture_from_file", "Tried to load texture map '%s' from file path '%s', even though it is embedded! Bailing.\n", temp_c_string(map->name), temp_c_string(map->full_path));
        return;
    }

    map->is_srgb = is_srgb;

    if (extension == String("dds"))
    {
        load_dds_texture_helper(map);
    }
    else
    {
        load_non_block_compressed_texture_helper(map);
    }
}

bool load_texture_from_memory(Texture_Map *map, u8 *memory, i64 size_to_read, bool is_srgb)
{
    i32 width, height, channels_count;
    u8 *data = stbi_load_from_memory(memory, size_to_read, &width, &height, &channels_count, 0);

    map->is_embedded = true; // Tag the Texture_Map that is is an embedded image.

    if (!data)
    {
        logprint("load_texture_from_memory", "Failed to load embedded image '%s' from memory!\n", temp_c_string(map->name));
        return false;
    }

    Bitmap bitmap;
    bitmap.width  = width;
    bitmap.height = height;
    bitmap.data   = data;

    map->format = texture_format_with_this_nchannels(channels_count);

    map->is_srgb = is_srgb;
    map->num_mipmap_levels = 1; // @Hardcoded: How to know?
    update_texture_from_bitmap(map, &bitmap);
    
    return true;
}

void reload_asset(Texture_Catalog *catalog, Texture_Map *map)
{
    if (!map->loaded)
    {
        init_texture_map(map);
    }

    if (!map->full_path)
    {
        logprint("texture_catalog", "Received a texture map that did not have a full path! (%s)\n", temp_c_string(map->name));
        return;
    }

    load_texture_from_file(map, false);
}
