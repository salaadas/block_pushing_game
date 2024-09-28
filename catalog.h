#pragma once

#include "common.h"

#include "table.h"

//
// Todo: Document, as I sometimes forget about this.
//
// To summary, 'short_name' is the file name without the ending extension, and 'full_name' is the full path
// of the asset relative to the executable location.
//
//

struct Catalog_Base;

typedef void(*ProcRegisterLooseFile)(Catalog_Base *base, String short_name, String full_name);
typedef void(*ProcPerformReloadOrCreation)(Catalog_Base *base, String short_name, String full_name, bool do_load_asset);

//
// We separate Catalog_Base from Catalog, so that we can put Catalog_Base pointers into arrays, 
// pass to procedures, etc, without needing to know the full type of the Catalog.
//
struct Catalog_Base
{
    // @Note: Not needed right now because we are using the folder with the same name as my_name
    // RArr<String>                folders;
    RArr<String>                extensions;

    RArr<String>                short_names_to_reload;
    RArr<String>                full_names_to_reload;

    String                      my_name; // Name for this catalog

    ProcRegisterLooseFile       proc_register_loose_file        = NULL;
    ProcPerformReloadOrCreation proc_perform_reload_or_creation = NULL;
};

template <typename V>
struct Catalog
{
    Catalog_Base      base;
    Table<String, V*> table; // This guarantees pointer stability because for each of the entry, we do make_placeholder which New<>() an asset so that when the table resizes, we still have the valid pointer to the memory of the asset.
};

void   perform_reloads(Catalog_Base *base);
String get_extension(String filename);
void   catalog_loose_files(String folder, RArr<Catalog_Base*> *catalogs);

template <typename V>
void reload_asset(Catalog<V> *catalog, V *value);

template <typename V>
V *catalog_find(Catalog<V> *catalog, String short_name, bool log_on_not_found = true)
{
    auto tf      = table_find(&catalog->table, short_name);
    auto value   = tf.first;
    auto success = tf.second;

    if (!success || !value)
    {
        if (log_on_not_found) logprint("catalog", "Catalog '%s' was not able to find asset '%s'\n", catalog->base.my_name.data, temp_c_string(short_name));
        return NULL;
    }

    if (!value->loaded)
    {
        reload_asset(catalog, value);
        value->loaded = true;
    }

    return value;
}

template <class V> // @Important 'V' must be a catalog file type!
void my_register_loose_file(Catalog_Base *base, String short_name, String full_name)
{
    // Cannot use reinterpret_cast here? Idk why
    Catalog<V> *desired_catalog = (Catalog<V>*)base;

    // @Speed: Check if the thing exists before adding.
    {
        auto [dummy, found] = table_find(&desired_catalog->table, short_name);
        if (found)
        {
            auto path = dummy->full_path;
            if (path == full_name) return; // Bail because we already have the asset.
        }
    }

    V *new_catalog_file = make_placeholder(desired_catalog, short_name, full_name);

    // // Commenting these out because we only want to load file asset on demand.
    // reload_asset(desired_catalog, new_catalog_file);
    // new_catalog_file->loaded = true;

    String table_key = copy_string(new_catalog_file->name);
    table_add(&desired_catalog->table, table_key, new_catalog_file);
}

// @Incomplete: We should check if we don't want to do_load_asset, then only create the file and exit.
// However, our usage has not involved such cases yet, so that's @Todo for later.
template <class V> // @Important 'V' must be a catalog file type!
void my_perform_reload_or_creation(Catalog_Base *base, String short_name, String full_name, bool do_load_asset)
{
    // Cannot use reinterpret_cast here? Idk why
    Catalog<V> *desired_catalog = (Catalog<V>*)base;

    auto pointer = table_find_pointer(&desired_catalog->table, short_name);

    if (!pointer)
    {
        logprint("catalog", "Making new asset '%s' at path '%s'!\n", temp_c_string(short_name), temp_c_string(full_name));
        base->proc_register_loose_file(base, short_name, full_name);
    }
    else
    {
        logprint("catalog", "Reloading asset '%s' of catalog '%s'!\n", temp_c_string(short_name), temp_c_string(base->my_name));

        // We leave the freeing part up to the reload_asset procedure! So the catalogs must take responsibility for this.
        reload_asset(desired_catalog, *pointer);
    }
}

template <class V> // @Important 'V' must be a catalog file type!
void do_polymorphic_catalog_init(Catalog<V> *catalog)
{
    catalog->base.proc_register_loose_file        = my_register_loose_file<V>;
    catalog->base.proc_perform_reload_or_creation = my_perform_reload_or_creation<V>;
    
    init(&catalog->table);
}
