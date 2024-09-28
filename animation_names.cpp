#include "animation_names.h"

#include "catalog.h"
#include "file_utils.h"

// RArr<Animation_Names*> dirty_assets; // This is gonna be global for a while, since we are only having one Animation_Names catalog for now...

void init_animation_names_catalog(Animation_Names_Catalog *catalog)
{
    catalog->base.my_name = String("Animation Names");
    array_add(&catalog->base.extensions, String("animation_names"));
    do_polymorphic_catalog_init(catalog);
}

Animation_Names *make_placeholder(Animation_Names_Catalog *catalog, String short_name, String full_name)
{
    auto names       = New<Animation_Names>();
    names->name      = copy_string(short_name);
    names->full_path = copy_string(full_name);

    return names;
}

void reload_asset(Animation_Names_Catalog *catalog, Animation_Names *animation_names)
{
    if (animation_names->name_to_anim_name.count)
    {
        table_reset(&animation_names->name_to_anim_name);
    }

    Text_File_Handler handler;
    String agent("Animation Names");

    start_file(&handler, animation_names->full_path, agent);
    if (handler.failed) return;
    defer { deinit(&handler); };

    while (true)
    {
        auto [line, found] = consume_next_line(&handler);

        if (!found) break;
        assert(line);

        auto [name, anim_name] = break_by_spaces(line);

        // Don't add the mapping if the anim name is of the character '-'.
        // Also don't add if there is no name for the Sampled_Animation.
        if (anim_name && anim_name != String("-"))
        {
            table_add(&animation_names->name_to_anim_name, copy_string(name), copy_string(anim_name));
        }
    }
}

// void clean_dirty_names()
// {
//     // for (auto it : dirty_assets)
//     // {
//     //     ensure_updated(it);
//     // }

//     array_reset(&dirty_assets);
// }
