// https://developer.ibm.com/tutorials/l-inotify/

#include "hotloader.h"

#include "main.h"
#include "file_utils.h"
#include "time_info.h"

constexpr f64 RELOAD_DELAY = 0.1;
constexpr u32 READ_LEN     = 4096;

bool hotloader_initted = false;

RArr<String>         directory_names;
RArr<Directory_Info> directories; // Directories to watch
RArr<Asset_Change>   asset_changes;

u32 total_changes_processed = 0;

Hotloader_Callback hotloader_callback;



#include <unistd.h>
#include <sys/inotify.h>

i32 inotify_instance = -1;

void add_subdirectories_to_list(String short_name, String full_name, void *data)
{
    array_add_if_unique(&directory_names, copy_string(full_name));
    visit_files(full_name, NULL, add_subdirectories_to_list, true);
}

void hotloader_init()
{
    assert((!hotloader_initted));
    hotloader_initted = true;

    inotify_instance = inotify_init1(IN_NONBLOCK);
    assert((inotify_instance != -1));

    String root_data_directory("data");

    array_add_if_unique(&directory_names, copy_string(root_data_directory));
    visit_files(root_data_directory, NULL, add_subdirectories_to_list, true);

    for (auto dir_name : directory_names)
    {
        Directory_Info info;

        // Init and allocate

        auto c_path = (char*)temp_c_string(dir_name);
        info.watch_descriptor = inotify_add_watch(inotify_instance, c_path, IN_MODIFY | IN_CREATE | IN_DELETE);

        if (info.watch_descriptor == -1)
        {
            logprint("hotloader_init", "Could not add watch for directory '%s'.\n", c_path);
        }

        info.name              = dir_name;
        info.read_issue_failed = true;

        array_add(&directories, info);
    }
}

void release(Directory_Info *info);

void hotloader_shutdown()
{
    if (!hotloader_initted) return;

    for (auto &it : directories) // This is taking the reference
        release(&it);            // This is taking the pointer

    array_reset(&directories);

    close(inotify_instance);

    hotloader_initted = false;
}

void hotloader_register_callback(Hotloader_Callback callback)
{
    hotloader_callback = callback;
}

// void issue_one_read(Directory_Info *info)
// {
//     auto len = read(inotify_instance, info->event, READ_LEN);

//     if (len == -1)
//     {
//         // printf("[pump_linux_notifications]: Failed to issue a read.\n");
//         info->read_issue_failed = true;
//         info->bytes_returned    = 0;
//         info->event             = NULL;
//         return;
//     }

//     auto event = (struct inotify_event*)info->event;

//     if (event->mask & IN_ISDIR)
//     {
//         printf("[pump_linux_notifications]: It is a directory.\n");
//         // Don't care about directory changes.
//         info->read_issue_failed = true;
//         info->bytes_returned    = 0;
//         info->event             = NULL;
//         return;
//     }
//     else
//     {
//         printf("[pump_linux_notifications]: It is a file.\n");
//         info->read_issue_failed = false;
//         info->bytes_returned    = len;
//         info->event             = (u8*)event;
//         return;
//     }
// }

// @Important: Destructive to input string's data
my_pair<String, String> chop_and_lowercase_extension(String short_name)
{
    auto pos = find_index_from_right(short_name, '.');

    if (pos != -1)
    {
        String s_pass_pos = short_name;
        advance(&s_pass_pos, pos + 1);

        for (i32 i = 0; i < s_pass_pos.count; ++i)
        {
            auto c = s_pass_pos[i];
            if (('A' <= c) && (c <= 'Z'))
            {
                auto n = c - 'A';
                s_pass_pos.data[i] = 'a' + n;
            }
        }

        auto basename = short_name;
        basename.count = short_name.count  - 1 - s_pass_pos.count;

        return {basename, s_pass_pos};
    }
    else
    {
        return {short_name, String("")};
    }
}

void pump_linux_notifications()
{
    // @Hack: Cleanup any better way?
    char buffer[READ_LEN];

    auto len = read(inotify_instance, buffer, sizeof(buffer));

    if (len == -1)
    {
        // printf("[pump_linux_notifications]: Failed to issue a read.\n");
        return;
    }

    auto event = (struct inotify_event*)buffer;

    if (event->mask & IN_ISDIR)
    {
        printf("[pump_linux_notifications]: It is a directory.\n");
        return;
    }
    else
    {
        // printf("[pump_linux_notifications]: It is a file.\n");
    }

    if (event == NULL)
    {
        printf(" ************ WHY ARE YOU HERE\n");
        return;
    }

    for (auto &info : directories)
    {
        if (event->wd == info.watch_descriptor)
        {
            // issue_one_read(&info);
            // Read the next event immediately

            // logprint("pump_linux_notifications", "Event name: '%s'.\n", event->name);

            String short_name = sprint(String("%s"), event->name);
            auto [name_no_ext, ext] = chop_and_lowercase_extension(short_name);

            bool reject = false;

            auto count = name_no_ext.count;
            if (count == 0) reject = true;

            if (count >= 1)
            {
                if (short_name[0] == '.') reject = true;
            }

            if (count >= 2)
            {
                if ((short_name[0]) == '.' && (short_name[1] == '#')) reject = true;
            }

            if (count >= 2)
            {
                if ((short_name[0]) == '#' && (short_name[short_name.count - 1] == '#')) reject = true;
            }

            if (reject)
            {
                // logprint("hotloader", "Reject changes to file '%s'.\n", temp_c_string(short_name));
                free_string(&short_name);
                continue;
            }

            if (ext.count)
            {
                String full_name = sprint(String("%s/%s"), temp_c_string(info.name), event->name);
                String extension = ext;

                auto now = get_time();

                for (auto &change : asset_changes)
                {
                    if ((change.short_name == short_name) && (change.full_name == full_name) && (change.extension == extension))
                    {
                        if (change.processed)
                        {
                            logprint("hotloader", "            Rare case\n");
                            change.processed = false;
                            change.time_of_last_change = now;

                            total_changes_processed -= 1;
                        }

                        return;
                    }
                }

                Asset_Change change;
                change.short_name = short_name;
                change.full_name  = full_name;
                change.extension  = extension;
                change.processed  = false;
                change.time_of_last_change = now;

                // printf("Adding asset %s, full %s ext %s to array\n", temp_c_string(change.short_name),
                //        temp_c_string(change.full_name),
                //        temp_c_string(change.extension));
                array_add(&asset_changes, change);
            }
            else
            {
                // logprint("hotloader", "Reject changes to file '%s'.\n", temp_c_string(short_name));
                free_string(&short_name);
            }
        }
    }
}

bool hotloader_process_change()
{
    if (!hotloader_initted) hotloader_init();

    // for (auto &info : directories)
    // {
    //     if (info.read_issue_failed)
    //     {
    //         info.read_issue_failed = false;
    //         issue_one_read(&info);
    //         // If still failed, bail
    //         if (info.read_issue_failed)
    //         {
    //             // logprint("hotloader", "No changes happened to folder '%s'.\n", temp_c_string(info.name));
    //             return false;
    //         }
    //     }
    // }

    pump_linux_notifications();

    if (!asset_changes.count)
    {
        return false;
    }

    auto now = get_time();

    for (auto &change : asset_changes)
    {
        if (change.processed) continue;

        auto delta = now - change.time_of_last_change;
        if (delta < RELOAD_DELAY)
        {
            continue;
        }

        auto handled = false;

        // Do stuff with catalogs and set handled = true if handled here
        auto [name_no_ext, ext] = chop_and_lowercase_extension(change.short_name);
        auto full_path  = change.full_name;
        for (auto it : all_catalogs)
        {
            if (!ext) break;

            auto found = array_find(&it->extensions, ext);
            if (found)
            {
                array_add(&it->short_names_to_reload, name_no_ext);
                array_add(&it->full_names_to_reload,  full_path);

                printf("Handling the file '%s' over to catalog '%s'.\n",
                       temp_c_string(name_no_ext), temp_c_string(it->my_name));

                perform_reloads(it);

                handled = true;
                break;
            }
        }

        // Otherwise revert to the callback
        if (hotloader_callback != NULL)
        {
            hotloader_callback(&change, handled);
        }

        // Issue a remove for the asset change.
        // ....

        change.processed = true;
        total_changes_processed += 1;
        // After done with an asset, we free it's memory
        free_string(&change.short_name);
        free_string(&change.full_name);

        if (total_changes_processed == asset_changes.count)
        {
            // printf("[hotloader]: Processed all asset changes, reseting the changes array.\n");
            total_changes_processed = 0;
            array_reset(&asset_changes);
        }

        return true; // One change at a time
    }

    return false;
}

void release(Directory_Info *info)
{
    free_string(&info->name);
    inotify_rm_watch(inotify_instance, info->watch_descriptor);
}
