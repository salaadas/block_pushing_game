#pragma once

#include "common.h"

#include "catalog.h"
#include "shader_catalog.h"
#include "texture_catalog.h"
#include "level_set.h"
#include "mesh_catalog.h"

extern String              dir_of_running_exe;

extern RArr<Catalog_Base*> all_catalogs;
extern Shader_Catalog      shader_catalog;
extern Texture_Catalog     texture_catalog;
extern Level_Set_Catalog   level_set_catalog;
extern Mesh_Catalog        mesh_catalog;

extern Texture_Map *white_texture;

extern bool was_window_resized_this_frame;

#define DEVELOPER_MODE

extern const String LEVEL_SET_NAME;
extern const String OVERRIDE_LEVEL_NAME;

// extern const String SOUND_FOLDER;
// extern const String MUSIC_FOLDER;
extern const String FONT_FOLDER;
extern          i32 BIG_FONT_SIZE;

enum Program_Mode
{
    GAME,
    EDITOR,
};

extern Program_Mode program_mode;
