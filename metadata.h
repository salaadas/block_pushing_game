#pragma once

#include "common.h"

#include "entity_manager.h"

struct Metadata_Item;

struct Metadata
{
    _type_Type entity_type = _make_Type(Entity);  // @Ughh: @Fixme: Get rid of this.

    RArr<Metadata_Item*> leaf_items;
    RArr<Metadata_Item*> top_level_items;
};

struct Metadata_Item
{
    Metadata *metadata;
    i64 byte_offset;

    i64 runtime_size;

    // Only used with array Metadate_Item
    // i64 element_runtime_size;

    // Dynamically allocated for now.
    String name;
    // String ui_name;
    // String description;

    bool imported_from_base_entity;

    enum Item_Info : u8
    {
        UNKNOWN = 0,
        POD,
        STRING,
        TYPE,
        PID,              // Not handled
        DYNAMIC_ARRAY,    // Not handled
        STATIC_ARRAY,     // Not handled
        COUNT
    };
    Item_Info info;
};

extern RArr<Metadata*> metadata_per_entity_type;
extern Metadata base_entity_metadata;

i64 entity_manager_index_of_type(_type_Type entity_type);
i64 get_local_field_index(Metadata *metadata, i64 field_index);
u8 *metadata_slot(Entity *e, Metadata_Item *item);

void init_metadata_for_all_entities_type();

i64 get_runtime_size_of_entity(_type_Type entity_type);
