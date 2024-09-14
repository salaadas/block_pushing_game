#include "catalog.h"

#include "file_utils.h"

void perform_reloads(Catalog_Base *base)
{
    assert(base->short_names_to_reload.count == base->full_names_to_reload.count);

    i64 index = 0;
    for (auto it : base->short_names_to_reload)
    {
        auto short_name = copy_string(it);
        auto full_name  = copy_string(base->full_names_to_reload[index]);

        base->proc_perform_reload_or_creation(base, short_name, full_name, true);

        index += 1;

        // @Cleanup: allocated these in temporary storage.
        free_string(&short_name);
        free_string(&full_name);
    }

    array_reset(&base->short_names_to_reload);
    array_reset(&base->full_names_to_reload);
}

String get_extension(String filename)
{
    auto index = find_index_from_right(filename, '.');

    if (index == -1)
    {
        return String("");
    }

    index += 1;

    String result = copy_string(filename, {global_context.temporary_storage, __temporary_allocator});
    result.data += index;
    result.count -= index;

    return result;
}

#include "main.h"
void add_relevent_files_and_parse(String short_name, String full_name, void *data)
{
    auto highest_water = get_temporary_storage_mark();
    defer { set_temporary_storage_mark(highest_water); };

    auto catalog = (Catalog_Base*)data;
    auto extensions_list = (RArr<String>*)(&catalog->extensions);
    String ext = get_extension(short_name);

    // @Speed:
    // If it is a directory, continue to traverse.
    if (!ext || !array_find(extensions_list, ext))
    {
        visit_files(full_name, catalog, add_relevent_files_and_parse);
        return;
    }

    short_name.count -= 1;
    short_name.count -= ext.count;

    // Be sure to copy_string the names in your proc because short_name is allocated in temp storage
    catalog->proc_register_loose_file(catalog, short_name, full_name);
}

// Each type of catalog will have its own way of registering loose file
//   or performing reload. @Important: THESE PROCEDURES MUST BE DEFINE BY YOU
// Because of this, when we load them, parse the data and such.
void catalog_loose_files(String root_folder, RArr<Catalog_Base*> *catalogs)
{
    // printf("Entering '%s' to do work...\n", folder.data);
    // printf("Count of catalogs: %ld\n", catalogs->count);

    for (auto it : *catalogs)
    {
        // Recursively visits each and every folder.
        bool success = visit_files(root_folder, it, add_relevent_files_and_parse);
        if (!success)
        {
            auto path = root_folder;

            fprintf(stderr, "Could not visit %s...\n", path.data);
            assert(0);
        }
    }
}
