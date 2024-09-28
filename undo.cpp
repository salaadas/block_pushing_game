// @Leak: We don't ever free the data from the undo system
// @Leak: We don't ever free the data from the undo system
// @Leak: We don't ever free the data from the undo system

// @Fixme: Rewrite undo! And remove memcpy
// @Fixme: Rewrite undo! And remove memcpy
// @Fixme: Rewrite undo! And remove memcpy
// @Fixme: Rewrite undo! And remove memcpy

#include "undo.h"

#include "metadata.h"
#include "string_builder.h"
#include "sokoban.h"
#include "file_utils.h" // for advance(); // @Cleanup
#include "hud.h" // for game_report()

// For now, we don't care about BIG_ENDIAN machines
constexpr bool TARGET_IS_LITTLE_ENDIAN = true;

// These assumes that the derived entity classes has a field of base entitiy.
constexpr u8 STRUCT_INDEX_BASE = 0;
constexpr u8 STRUCT_INDEX_DERIVED = 1;

bool doing_undo = false;

void put_n_bytes_with_endian_swap(String_Builder *builder, u8 *old, u8 *current, i64 size)
{
    // if (TARGET_IS_LITTLE_ENDIAN)
    // {
        append(builder, old, size);
        append(builder, current, size);
    // }
    // else
    // {
    //     append(builder, old, size, true);
    //     append(builder, current, size, true);
    // }
}

template <typename T>
void put(String_Builder *builder, T x)
{
    static_assert((std::is_arithmetic<T>::value || std::is_enum<T>::value), "T must be arithmetic type or enum");

    auto size = sizeof(T);
    ensure_contiguous_space(builder, size);

    // if (TARGET_IS_LITTLE_ENDIAN)

    auto current_buffer = builder->current_buffer;
    memcpy(current_buffer->data.data + current_buffer->occupied, &x, size);
    current_buffer->occupied += size;
}

void put(String_Builder *builder, String s)
{
    if (s.data == NULL)
    {
        assert((s.count == 0));
    }

    put(builder, s.count);
    append(builder, s.data, s.count);
}

template <typename T>
void get(String *s, T *x)
{
    static_assert((std::is_arithmetic<T>::value || std::is_enum<T>::value), "T must be arithmetic type or enum");

    auto size = sizeof(T);
    assert((s->count >= size));

    memcpy(x, s->data, size);

    s->data  += size;
    s->count -= size;
}

bool consume_u8_and_length(String *src, u8 *dest, i64 count)
{
    if (count < 0) return false;
    if (count > src->count) return false;
    if (count == 0) return true;

    memcpy(dest, src->data, count);

    src->data  += count;
    src->count -= count;

    return true;
}

void get(String *src, String *dest)
{
    i64 count;
    get(src, &count);

    assert((count >= 0));

    dest->count = count;
    dest->data  = reinterpret_cast<u8*>(my_alloc(count));

    if (!dest->data) return; // @Incomplete: Log error
    auto success = consume_u8_and_length(src, dest->data, count);

    if (!success)
    {
        printf("NOT ENOUGH ROOM LEFT IN STRING!\n");
        assert(0);
    }
}

inline
void discard_string(String *s)
{
    i64 count;
    get(s, &count);

    assert((count >= 0));

    advance(s, count);
}

inline
void extract_string(String *src, String *dest)
{
    // @Incomplete: Not sure if it's safe to do this, so we have a @Leak for now. :ImplementMe
    // free_string(dest);
    i64 count;
    get(src, &count);

    dest->data = reinterpret_cast<u8*>(my_alloc(count));
    assert(dest->data);

    dest->count = count;
    memcpy(dest->data, src->data, count);

    advance(src, count);
}

void apply_diff(u8 entity_type_index, Entity *e_dest, u8 number_of_slots_changed, String *transaction, bool apply_forward) // @Speed: make the apply_diff 2 different procedures because we don't want to keep checking for apply_forward
{
    for (i64 i = 0; i < number_of_slots_changed; ++i)
    {
        u8 struct_index;
        get(transaction, &struct_index);

        u8 field_index;
        get(transaction, &field_index);

        // :ImplementMe :VersionHandling Look this up in the Type_Manifest.

        Metadata *metadata;
        if (struct_index == 0) metadata = &base_entity_metadata;
        else metadata = metadata_per_entity_type[entity_type_index];

        auto item = metadata->leaf_items[field_index];

        auto slot_dest = metadata_slot(e_dest, item);

        if (item->info == Metadata_Item::POD)
        {
            auto size = item->runtime_size;

            if (apply_forward)
            {
                advance(transaction, size);                 // Discard old value.
                memcpy(slot_dest, transaction->data, size); // Apply new value
                advance(transaction, size);                 // Advance past new value.
            }
            else
            {
                memcpy(slot_dest, transaction->data, size); // Apply old value
                advance(transaction, size);                 // Advance past old value.
                advance(transaction, size);                 // Discard new value.
            }
        }
        else if (item->info == Metadata_Item::STRING)
        {
            auto string_dest = reinterpret_cast<String*>(slot_dest);

            if (apply_forward)
            {
                discard_string(transaction);
                extract_string(transaction, string_dest);
            }
            else
            {
                extract_string(transaction, string_dest);
                discard_string(transaction);
            }
        }
        else
        {
            printf("Unhandled metadata info item type %d\n", item->info);
            assert(0);
        }
    }    
}

struct Pack_Info
{
    String_Builder *builder;
    u8 *pointer_to_slot_count = NULL;
    u8  slot_count = 0;
};

inline
u8 *get_cursor(String_Builder *builder)
{
    return builder->current_buffer->data.data + builder->current_buffer->occupied;
}

inline
void increment_pack_count(Pack_Info *info, Entity *entity)
{
    if (info->slot_count == 0)
    {
        auto type_index = entity_manager_index_of_type(entity->type);

        // Before careful of the size of the types of entities.
        // Putting the type index for the entity before putting the
        // placeholder for the slot count.
        put(info->builder, static_cast<u8>(type_index));

        // Storing the id of the entity.
        put(info->builder, static_cast<u32>(entity->entity_id));

        auto success = ensure_contiguous_space(info->builder, 1);
        if (!success)
        {
            assert(0);
        }

        info->pointer_to_slot_count = get_cursor(info->builder);
        put(info->builder, static_cast<u8>(0)); // Placeholder for the slot count to be overwritten in the future.
    }

    info->slot_count += 1;
}

inline
void compare_metadata_items(Entity *e_old, Entity *e_new, u8 struct_index, Metadata_Item *item, i64 index, Pack_Info *info)
{
    auto slot_old = metadata_slot(e_old, item);
    auto slot_new = metadata_slot(e_new, item);

    auto size = item->runtime_size;
    auto differing = memcmp(slot_old, slot_new, size);
    
    if (differing)
    {
        auto builder = info->builder;

        if (item->info == Metadata_Item::POD)
        {
            increment_pack_count(info, e_old);

            // printf("DIFFERENT '%s'\n", temp_c_string(item->name));
            put(builder, struct_index);

            // @Incomplete: Version handling for forward/backward compability when we load
            // a save game from an older version. We need to know which field of the old version
            // maps to the field in the new version. (Using Type_Manifest)
            //
            // We'll do the this when we start doing revision number for our entities.
            u8 field_index = static_cast<u8>(index);

            put(builder, field_index);
            put_n_bytes_with_endian_swap(builder, slot_old, slot_new, size);
        }
        else if (item->info == Metadata_Item::STRING)
        {
            // For the case of strings, the memcmp above was a conservative detection.
            // If the strings are byte-equal, then they are obviously equal. But if
            // they're not, they could point to two strings of the same length but
            // with differing contents. That is what we check here.

            auto s_old = reinterpret_cast<String*>(slot_old);
            auto s_new = reinterpret_cast<String*>(slot_new);
 
            if (*s_old != *s_new)
            {
                increment_pack_count(info, e_old);

                // printf("DIFFERENT '%s'\n", temp_c_string(item->name));
                put(builder, struct_index);

                // @Incomplete: Check above
                u8 field_index = static_cast<u8>(index);

                put(builder, field_index);

                put(builder, *s_old);
                put(builder, *s_new);
            }
        }
        else
        {
            printf("Field '%s' has unsupported type!\n", temp_c_string(item->name));
            assert(0);
        }
    }
}

void diff_entity(Entity *e_old, Entity *e_new, Pack_Info *pack_info)
{
    assert((e_old->type == e_new->type));

    auto index    = entity_manager_index_of_type(e_old->type);
    auto metadata = metadata_per_entity_type[index];
    assert((metadata != NULL));

    auto num_fields = metadata->leaf_items.count;
    // Because visit_struct limits to 69
    assert((num_fields <= 69));

    i64 item_index = 0;
    for (auto item : base_entity_metadata.leaf_items)
    {
        compare_metadata_items(e_old, e_new, STRUCT_INDEX_BASE, item, item_index, pack_info);
        item_index += 1;
    }

    // We need to somehome differentiate between the base and the derived class?

    item_index = 0;
    for (auto item : metadata->leaf_items)
    {
        compare_metadata_items(e_old, e_new, STRUCT_INDEX_DERIVED, item, item_index, pack_info);
        item_index += 1;
    }

    if (pack_info->slot_count)
    {
        assert(pack_info->pointer_to_slot_count);

        *pack_info->pointer_to_slot_count = pack_info->slot_count;
    }
}

inline
void copy_slot(Entity *src, Entity *dest, Metadata_Item *item)
{
    if (item->info == Metadata_Item::POD)
    {
        auto slot_src  = metadata_slot(src, item);
        auto slot_dest = metadata_slot(dest, item);

        memcpy(slot_dest, slot_src, item->runtime_size);
    }
    else if (item->info == Metadata_Item::TYPE)
    {
        printf("Not supposed to copy type!\n");
        assert(0);

        // We straight-up do a shallow clone if we decide to clone _type_Type
        auto slot_src  = metadata_slot(src, item);
        auto slot_dest = metadata_slot(dest, item);

        memcpy(slot_dest, slot_src, item->runtime_size);
    }
    else if (item->info == Metadata_Item::STRING)
    {
        auto slot_src  = reinterpret_cast<String*>(metadata_slot(src, item));
        auto slot_dest = reinterpret_cast<String*>(metadata_slot(dest, item));

        if (slot_dest->count) free_string(slot_dest);

        *slot_dest = copy_string(*slot_src);
    }
    // @Note: Make sure the array only contains POD elements! (see file metadata.cpp)
    else if (item->info == Metadata_Item::STATIC_ARRAY)
    {
        assert(0);
    }
    // @Incomplete: Not tested
    else if (item->info == Metadata_Item::DYNAMIC_ARRAY)
    {
        // This assumes the array contains POD elements.
        auto src_array  = reinterpret_cast<RArr<u8>*>(metadata_slot(src, item));
        auto dest_array = reinterpret_cast<RArr<u8>*>(metadata_slot(dest, item));

        array_reset(dest_array);

        // I guess the dest_array should have the same allocator?
        dest_array->allocator.proc = src_array->allocator.proc;
        dest_array->allocator.data = src_array->allocator.data;

        dest_array->allocated = 0;
        dest_array->count = 0;

        array_reserve(dest_array, src_array->count);
        dest_array->count = src_array->count;

        memcpy(dest_array->data, src_array->data, src_array->count);
    }
    else
    {
        printf("In struct '%s', copying of field '%s' has unsupported type!\n", src->type.name(), temp_c_string(item->name));
        assert(0);
    }
}

void copy_entity_data(Entity *src, Entity *dest)
{
    assert((src->type == dest->type));

    // dest->entity_flags = src->entity_flags;

    auto m_index = entity_manager_index_of_type(src->type);
    auto m = metadata_per_entity_type[m_index];

    for (auto item : base_entity_metadata.top_level_items)
    {
        // @Incomplete :VersionHandling
        copy_slot(src, dest, item);
    }

    for (auto item : m->top_level_items)
    {
        // @Incomplete :VersionHandling
        copy_slot(src, dest, item);
    }
}

Entity *clone_entity_via_alloc(Entity *e)
{
    auto runtime_size = get_runtime_size_of_entity(e->type);
    auto result = reinterpret_cast<Unknown_Entity*>(my_alloc(runtime_size));

    // Not initting because we will copy the fields of 'e' to 'result'
    result->base = New<Entity>(false);
    result->base->type = e->type;
    result->base->derived_pointer = result;

    result->base->entity_id = e->entity_id;
    copy_entity_data(e, result->base);
    
    return result->base;
}

void init_undo_handler(Undo_Handler *handler)
{
    handler->manager = NULL;

    array_init(&handler->undo_records);

    // @Leak: Not freeing the table
    init(&handler->cached_entity_state);

    handler->dirty   = false;
    handler->enabled = false;
}

void really_do_one_undo(Entity_Manager *manager, Undo_Record *record, bool is_redo)
{
    doing_undo = true;
    defer { doing_undo = false; };
    
    auto remaining = record->transactions;
    while (remaining)
    {
        u8 action;
        get(&remaining, &action);

        if (action == Undo_Action_Type::CHANGE)
        {
            u16 num_entities;
            get(&remaining, &num_entities);

            // @Note: Do entity changes
            {
                auto handler = manager->undo_handler;

                game_report(tprint(String("Undoing %u entities."), num_entities));

                for (i64 i = 0; i < num_entities; ++i)
                {
                    u8 entity_type_index;
                    get(&remaining, &entity_type_index);

                    u32 entity_id;
                    get(&remaining, &entity_id);

                    u8 number_of_slots_changed;
                    get(&remaining, &number_of_slots_changed);

                    // :ImplementMe :VersionHandling Look this up in the Type_Manifest.

                    auto e_dest = find_entity(manager, static_cast<Pid>(entity_id));

                    assert((entity_type_index == entity_manager_index_of_type(e_dest->type)));

                    auto [cached, found] = table_find(&handler->cached_entity_state, static_cast<Pid>(entity_id));
                    assert(found);

                    // Remove the current world entity from the logical grid first
                    remove_from_grid(manager->proximity_grid, e_dest); // @Speed!!!!!!!!!!!

                    // Then apply the change to the cached version of the entity
                    apply_diff(entity_type_index, cached, number_of_slots_changed, &remaining, is_redo);

                    copy_entity_data(cached, e_dest);

                    // Add the world entity back to the grid
                    {
                        // @Cleanup: this maybe overkill
                        auto e = e_dest;
                        // auto new_position = e->position;
                        // move_entity(e, new_position, e->entity_id);
                        
                        add_to_grid(manager->proximity_grid, e_dest);
                        
                        // update_physical_position(e, new_position);
                        e->visual_position = e->position;
                        reset(&e->visual_interpolation);
                    }
                }
            }
        }
        else if (action == Undo_Action_Type::CREATION)
        {
            assert(0);
        }
        else if (action == Undo_Action_Type::DESTRUCTION)
        {
            assert(0);
        }
    }
}

void play_undo_events_forward(Entity_Manager *manager)
{
    auto handler = manager->undo_handler;

    for (auto it : handler->undo_records)
    {
        bool is_redo = true;
        really_do_one_undo(manager, it, is_redo);
    }
}

void clear_current_undo_frame(Undo_Handler *handler)
{
    // For now it does nothing, but it should clear the pending_destructions and pending_creations here.
}

void do_one_undo(Entity_Manager *manager)
{
    auto handler = manager->undo_handler;

    clear_current_undo_frame(handler);

    // Cehcking and performing one undo.
    {
        if (!handler->undo_records.count) return;

        // @Leak: Drop the record on the floor later.
        auto record = pop(&handler->undo_records);
        really_do_one_undo(manager, record, false);
    }
}

// @Fixme: We should consolidate this with undo_mark_beginning()
void reset_undo(Undo_Handler *handler)
{
    // @Incomplete @Leak: For now, we leak the undo_records, but we will
    // use a private allocator for undo and then drop the thing on the floor
    // once we are done.
    array_reset(&handler->undo_records);

    // @Leak the entities themselves; this won't matter eventually.
    table_reset(&handler->cached_entity_state);
}

inline
void scan_one_entity(Undo_Handler *handler, Entity *e, String_Builder *builder, i64 *counter)
{
    if (e->scheduled_for_destruction) return;

    // if (cmp_var_type_to_type(e->type, Entity_Issued_Sound)) return;

    auto e_old_ptr = table_find_pointer(&handler->cached_entity_state, e->entity_id);
    assert((e_old_ptr != NULL));

    auto e_old = *e_old_ptr;

    Pack_Info pack_info;
    pack_info.builder = builder;

    diff_entity(e_old, e, &pack_info);

    if (pack_info.slot_count)
    {
        *counter += 1;

        // Update the entity state in the current_entity_stat table.
        // @Leak: Dropping e on the floor. Free it.
        auto clone = clone_entity_via_alloc(e);
        *e_old_ptr = clone;
    }
}

void scan_for_changed_entities(Undo_Handler *handler, String_Builder *builder)
{
    auto manager = handler->manager;
    assert(manager);

    i64 counter = 0; // Expected to max at u16

    put(builder, Undo_Action_Type::CHANGE);
    auto success = ensure_contiguous_space(builder, 1);
    assert(success);

    auto entity_count_cursor = get_cursor(builder);
    put(builder, static_cast<u16>(0)); // This is a placeholder for the future entity count.

    // @Fixme: If for some reason scan_one_entity does not add detect any changes we are still
    // return a 3 bytes long string because of the above 2 puts. We may need to perform a check here
    // to see if the scan_one_entity did anything or not and return an empty string.
    // Maybe not! And we could just selectively choose where to call undo_end_frame.
    for (auto it : manager->all_entities)
    {
        scan_one_entity(handler, it, builder, &counter);
    }

    {
        auto lo = static_cast<u8>(counter & 0xff);
        auto hi = static_cast<u8>((counter >> 8) & 0xff);

        // Little endian
        entity_count_cursor[0] = lo;
        entity_count_cursor[1] = hi;
    }
}

void undo_mark_beginning(Entity_Manager *manager)
{
    auto handler = manager->undo_handler;
    handler->enabled = true;
    handler->manager = manager;

    // :ImplementMe ... reset noted things?

    // @Incomplete @Leak: For now, we leak the undo_records, but we will
    // use a private allocator for undo and then drop the thing on the floor
    // once we are done.

    for (auto e : manager->all_entities)
    {
        if (e->scheduled_for_destruction) continue;

        auto clone = clone_entity_via_alloc(e);

        assert((clone->entity_id == e->entity_id));
        table_add(&handler->cached_entity_state, e->entity_id, clone);
    }
}

void init_undo_record(Undo_Record *record)
{
    // record->checkpoint = false;
    record->gameplay_time = -1;
}

#include "time_info.h" // For storing the record->gameplay_time.
void undo_end_frame(Undo_Handler *handler)
{
    if (!handler->enabled) return;

    String_Builder builder;

    scan_for_changed_entities(handler, &builder);

    // @Incomplete: Handle entity creation/destruction too, and add it to the builder.

    auto s = builder_to_string(&builder); // @Leak: We are not freeing the transactions.

    handler->dirty = true;

    auto record = New<Undo_Record>(false);
    init_undo_record(record);
    // @Fixme: @Temporary: We are currently using the current time as f32 to store the gameplay time. This might introduces weird issues later!
    record->gameplay_time = timez.current_time;
    record->transactions = s;

    array_add(&handler->undo_records, record);

    clear_current_undo_frame(handler);
}

#include "main.h" // For dir_of_running_exe
bool save_game(Entity_Manager *manager)
{
    String_Builder builder;
    // :VersionHandling Save the version of the current campaign.

    // If the mode has an advance, we just save the level index.
    // Otherwise, we save data.

    if (!manager) return false;

    auto handler = manager->undo_handler;
    if (!handler->dirty) return false;

    // save_type_manifest(&builder);

    put(&builder, handler->undo_records.count);

    for (auto record : handler->undo_records)
    {
        // put(&builder, record->checkpoint);

        auto serialized_data = record->transactions;
        put(&builder, serialized_data);
    }

    String file_to_save_as("sokosave.dat"); // @Hardcoded:
    auto file_save_path = tprint(String("%s/%s"), temp_c_string(dir_of_running_exe), file_to_save_as.data);
    auto c_path = reinterpret_cast<char*>(temp_c_string(file_save_path));

    auto file_descriptor = fopen(c_path, "wb");

    if (file_descriptor)
    {
        auto s = builder_to_string(&builder);

        auto written = fwrite(s.data, s.count, sizeof(u8), file_descriptor);
        assert((written == 1));

        fclose(file_descriptor);

        free_string(&s);
    }
    else
    {
        fprintf(stderr, "[save_game] Error: Unable to open file '%s' for writing saved game!\n", c_path);
    }

    return true;
}

bool load_game(Entity_Manager *manager)
{
    // @Hardcoded:
    String file_to_load("sokosave.dat"); // @Hardcoded:
    auto file_load_path = tprint(String("%s/%s"), temp_c_string(dir_of_running_exe), file_to_load.data);

    auto [s, success] = read_entire_file(file_load_path);
    if (!success) return false;

    auto orig = s;
    defer { free_string(&orig); };

    // :VersionHandling check for campaign versions

    if (!manager) return false;

    auto handler = manager->undo_handler;
    handler->dirty = false; // Hey, we are writing into it.

    // auto type_manifest = load_type_manifest(&s);

    i64 records_count;
    get(&s, &records_count);

    for (i64 it_index = 0; it_index < records_count; ++it_index)
    {
        auto record = New<Undo_Record>(false);
        init_undo_record(record);
        // get(&s, &record->checkpoint);

        i64 serialized_bytes;
        get(&s, &serialized_bytes);

        if (s.count < serialized_bytes)
        {
            fprintf(stderr, "[load_game] Error: Not enough bytes (%ld) to read the undo record (%ld bytes)!\n", s.count, serialized_bytes);
            return false;
        }

        auto t = s;
        t.count = serialized_bytes;

        advance(&s, serialized_bytes);

        record->transactions = copy_string(t);

        array_add(&handler->undo_records, record);
    }

    play_undo_events_forward(manager);

    printf("[load_game] Success! Loaded %ld records!\n", handler->undo_records.count);

    return true;
}
