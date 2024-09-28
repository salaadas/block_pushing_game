#include "level_set.h"

#include "file_utils.h"

void init_level_set(Level_Set *set)
{
    array_init(&set->level_names);

    set->loaded = false;
}

void init_level_set_catalog(Level_Set_Catalog *catalog)
{
    // @Fixme: Loading from the folder names 'my_name' for now
    catalog->base.my_name = String("level_sets");
    array_add(&catalog->base.extensions, String("level_set"));

    do_polymorphic_catalog_init(catalog);
}

Level_Set *make_placeholder(Level_Set_Catalog *catalog, String short_name, String full_path)
{
    Level_Set *set = New<Level_Set>(false);
    init_level_set(set);
    set->name      = copy_string(short_name);
    set->full_path = copy_string(full_path);

    return set;
}

// @Cleanup: Duplicate of entity_manager.cpp
#define error(c_agent, handler, ...)                                  \
    printf("[%s] Error at line %d: ", c_agent, handler.line_number);  \
    printf(__VA_ARGS__);                                              

void reload_asset(Level_Set_Catalog *catalog, Level_Set *set)
{
    array_reset(&set->level_names);

    Text_File_Handler handler;
    String agent("level_set:reload_asset");

    start_file(&handler, set->full_path, agent);
    if (handler.failed) return;

    defer { deinit(&handler); };

    auto c_agent = temp_c_string(agent);

    while (true)
    {
        auto [line, found] = consume_next_line(&handler);

        if (!found) break;

        assert(line);

        array_add(&set->level_names, copy_string(line));
    }    
}
