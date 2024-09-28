#pragma once

#include "common.h"
#include "table.h"

//
// Each animation name is a set of mappings from a preferred short name to a short name of a sampled animation file.
// This set of mappings is useful because the 'preferred short names' are used in the animation graph, where
// we specify how each animation should transition to each other.
//
struct Animation_Names
{
    String name;       // For catalog, name without extension of the animation_name file.
    String full_path;  // For catalog, path to the file, relative to the executable.
    bool   loaded = false; // For catalog

    // bool dirty = true;

    Table<String /*preferred name*/, String /*anim filename*/> name_to_anim_name;
};

template <typename V>
struct Catalog;

using Animation_Names_Catalog = Catalog<Animation_Names>;

void init_animation_names_catalog(Animation_Names_Catalog *catalog);
void reload_asset(Animation_Names_Catalog *catalog, Animation_Names *animation_names);
