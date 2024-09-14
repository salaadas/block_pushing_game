#pragma once

#include "common.h"

#include "catalog.h"

#include "texture.h"
using Texture_Catalog = Catalog<Texture_Map>;

void init_texture_catalog(Texture_Catalog *catalog);
void deinit_texture_catalog(Texture_Catalog *catalog);
Texture_Map *make_placeholder(Texture_Catalog *catalog, String short_name, String full_path);
void reload_asset(Texture_Catalog *catalog, Texture_Map *texture);
