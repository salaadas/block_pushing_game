#pragma once

#include "common.h"

#include "catalog.h"

struct Level_Set
{
    String       name;
    String       full_path;

    RArr<String> level_names;

    bool         loaded = false;
};

using Level_Set_Catalog = Catalog<Level_Set>;

void init_level_set_catalog(Level_Set_Catalog *catalog);
// void deinit_level_set_catalog(Level_Set_Catalog *catalog);
Level_Set *make_placeholder(Level_Set_Catalog *catalog, String short_name, String full_name);
void reload_asset(Level_Set_Catalog *catalog, Level_Set *set);
