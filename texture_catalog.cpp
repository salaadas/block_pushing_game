#include "texture_catalog.h"

#include <stb_image.h>
#include "opengl.h"

void init_texture_catalog(Texture_Catalog *catalog)
{
    // @Temporary @Hack
    // Set uv flip for stb here
    stbi_set_flip_vertically_on_load(true);

    catalog->base.my_name = String("textures");
    array_add(&catalog->base.extensions, String("jpg"));
    array_add(&catalog->base.extensions, String("png"));
    // array_add(&catalog->base.extensions, String("bmp"));
    // array_add(&catalog->base.extensions, String("tga"));

    do_polymorphic_catalog_init(catalog);
}

void deinit_texture_catalog(Texture_Catalog *catalog)
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

Texture_Map *make_placeholder(Texture_Catalog *catalog, String short_name, String full_path)
{
    Texture_Map *map = New<Texture_Map>();
    init_texture_map(map);
    map->name      = copy_string(short_name);
    map->full_path = copy_string(full_path);

    return map;
}

void reload_asset(Texture_Catalog *catalog, Texture_Map *map)
{
    if (!map->full_path)
    {
        fprintf(stderr, "Received a texture map that did not have a full path! (%s)\n",
                temp_c_string(map->name));
        return;
    }

    i32 width, height;
    i32 components;

    auto c_path = temp_c_string(map->full_path);

    u8 *data = stbi_load((char*)c_path, &width, &height, &components, 0);
    if (!data)
    {
        logprint("texture_catalog", "FAILED to load bitmap from path '%s'\n", c_path);
        return;
    }

    /*
    // @Temporary
    if (map->data)
    {
        stbi_image_free(map->data->data);
        my_free(map->data);
        map->data = NULL;
    }
    */

    // We leave the Bitmap attached for now.
    // This is probably a temporary thing...
    auto result = New<Bitmap>(false);
    result->width  = width;
    result->height = height;
    result->data   = data;
    result->length_in_bytes = width * height * components;

    // @Temporary @Hack @Fixme
    if (components == 3) result->format = Texture_Format::RGB888;
    else                 result->format = Texture_Format::ARGB8888;

    // @Cutnpaste from create_texture ??? @Fixme: we should make a create_texture
    map->width  = result->width;
    map->height = result->height;
    map->data   = result;

    update_texture(map);
}
