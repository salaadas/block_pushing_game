#pragma once

#include "common.h"

#include "catalog.h"

extern bool hotloader_initted;

struct Directory_Info
{
    String name;
    i32 watch_descriptor;
    bool read_issue_failed = false;
};

extern RArr<String>         directory_names;
extern RArr<Directory_Info> directories;

struct Asset_Change
{
    String short_name;
    String full_name;
    String extension;

    bool processed = false;

    f32 time_of_last_change = 0.0f;
};

typedef void(*Hotloader_Callback)(Asset_Change *change, bool handled);
extern Hotloader_Callback hotloader_callback;



void hotloader_init();
void hotloader_shutdown();
void hotloader_register_callback(Hotloader_Callback callback);
bool hotloader_process_change();
// void hotloader_register_catalog(Catalog_Base *catalog);
// void release(Asset_Change *change);
// void release(Directory_Info *info);

// @Important: Destructive to input string's data
my_pair<String, String> chop_and_lowercase_extension(String short_name);
