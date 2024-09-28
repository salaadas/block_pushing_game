#pragma once

#include "common.h"

#include "catalog.h"

#include "texture.h"
using Texture_Catalog = Catalog<Texture_Map>;

void init_texture_catalog(Texture_Catalog *catalog);
// void deinit_texture_catalog(Texture_Catalog *catalog);
Texture_Map *make_placeholder(Texture_Catalog *catalog, String short_name, String full_path);
void reload_asset(Texture_Catalog *catalog, Texture_Map *texture);

void load_texture_from_file(Texture_Map *map, bool is_srgb);
bool load_texture_from_memory(Texture_Map *map, u8 *memory, i64 size_to_read, bool is_srgb);

bool load_dds_texture_helper(Texture_Map *map);
