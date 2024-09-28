#pragma once

#include "common.h"

template <typename V>
struct Catalog;

struct Sampled_Animation;

using Animation_Catalog = Catalog<Sampled_Animation>;

void init_animation_catalog(Animation_Catalog *catalog);
Sampled_Animation *make_placeholder(Animation_Catalog *catalog, String short_name, String full_name);
void reload_asset(Animation_Catalog *catalog, Sampled_Animation *anim);
