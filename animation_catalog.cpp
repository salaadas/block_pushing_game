#include "animation_catalog.h"

#include "catalog.h"
#include "sampled_animation.h"

void init_animation_catalog(Animation_Catalog *catalog)
{
    catalog->base.my_name = String("Sampled Animation");

    array_add(&catalog->base.extensions, String("keyframe_animation"));

    do_polymorphic_catalog_init(catalog);
}

Sampled_Animation *make_placeholder(Animation_Catalog *catalog, String short_name, String full_name)
{
    auto anim       = New<Sampled_Animation>();
    anim->name      = copy_string(short_name);
    anim->full_path = copy_string(full_name);

    return anim;
}

void reload_asset(Animation_Catalog *catalog, Sampled_Animation *anim)
{
    if (anim->loaded)
    {
        logprint(temp_c_string(catalog->base.my_name), "Cannot hotload animation assets right now...!\n");
        assert(0);
    }

    auto success = load_sampled_animation(anim, anim->full_path);
    if (!success)
    {
        logprint(temp_c_string(catalog->base.my_name), "Failed to reload animation asset at path '%s'!\n", temp_c_string(anim->full_path));
    }
}
