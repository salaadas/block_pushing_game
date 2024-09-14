#include "metadata.h"

#include "visit_struct.h"

RArr<Metadata*> metadata_per_entity_type;
Metadata base_entity_metadata;

// Get the runtime size of different entities types
Table<_type_Type, i64> runtime_size_per_entity_type;

// @Important: This file should be the only place where we place all of the VISITABLE_STRUCT
// besides the entity_manager.h file

// Vectors :: Declare the serializable fields of the struct
VISITABLE_STRUCT(Vector2, x, y);
VISITABLE_STRUCT(Vector3, x, y, z);
VISITABLE_STRUCT(Vector4, x, y, z, w);
// Quaternion :: Declare the serializable fields of the struct
VISITABLE_STRUCT(Quaternion, x, y, z, w);
// String :: Declare the serializable fields of the struct
VISITABLE_STRUCT(String, count, data);

namespace vs = visit_struct;

// Make a hash table for this later
i64 entity_manager_index_of_type(_type_Type entity_type)
{
    // @Speed:
    auto probe = 0;
    for (auto it : metadata_per_entity_type)
    {
        if (entity_type == it->entity_type) return probe;
        probe += 1;
    }

    // Entity_Type must be in the metadata list, or else inspect the callsite!
    assert(0);
}

i64 get_local_field_index(Metadata *metadata, i64 field_index)
{
    auto count = field_index;

    i64 item_index = 0;
    for (auto item : metadata->leaf_items)
    {
        if (count == 0) return item_index;

        count -= 1;

        item_index += 1;
    }

    return -1;
}

template <typename V>
i64 array_sizeof_my_elements(RArr<V> *array)
{
    return sizeof(V);
}

// This is to make the compiler shut up
template <typename V>
i64 array_sizeof_my_elements(V *array)
{
    return -1;
}

u8 *metadata_slot(Entity *e, Metadata_Item *item)
{
    if (item->imported_from_base_entity)
    {
        auto p = reinterpret_cast<u8*>(e);
        return p + item->byte_offset;
    }
    else
    {
        auto p = reinterpret_cast<u8*>(e->derived_pointer);
        return p + item->byte_offset;
    }
}

// @Cleanup: Rename the derived_* variables, that is misleading
template <typename T>
void fill_metadata(T *derived_instance, Metadata *m,
                   bool from_base_entity,
                   i64 offset_relative_to_initial_instance,
                   bool is_top_level)
{
    u8 *derived_cursor = reinterpret_cast<u8*>(derived_instance);

    vs::for_each(*derived_instance, [&](const char *derived_field, auto &derived_ref_value) {
        u8 *member_pointer     = reinterpret_cast<u8*>(&derived_ref_value);
        i64 member_byte_offset = member_pointer - derived_cursor;
        i64 actual_byte_offset = member_byte_offset + offset_relative_to_initial_instance;
        // i64 element_runtime_size = 0;

        auto actual_value = derived_ref_value;
        auto probe_type = _make_Type(decltype(actual_value));

        Metadata_Item::Item_Info info = Metadata_Item::UNKNOWN;

        // Tagging the info for the item
        // @Speed: I suppose it does not matter much because we only call this once before the gameloop
        // But hey, it is worth optimizing if it is too slow.
        {
            auto name_of_type = String(probe_type.name());
            const String type_type_name("type_index");

            // Because these two structs are polymorphic structs, they need to be compared by names....
            const String dynamic_array_name("Resizable_Array");
            const String static_array_name("Static_Array");

            if (std::is_pod<decltype(actual_value)>())
            {
                info = Metadata_Item::POD;
            }
            else if (cmp_var_type_to_type(probe_type, String))
            {
                info = Metadata_Item::STRING;
            }
            else if (cmp_var_type_to_type(probe_type, Visual_Interpolation))
            {
                info = Metadata_Item::POD; // It was not pod because there is the Source_Location field inside visual_interpolation.
            }
            else if (contains(name_of_type, type_type_name))
            {
                info = Metadata_Item::TYPE;
                printf("Adding type as a metadata item for struct '%s', field '%s' (not supposed to happen).\n", _make_Type(*derived_instance).name(), derived_field);
            }
            // @Note: Make sure the array only contains POD elements!
            else if (contains(name_of_type, dynamic_array_name))
            {
                info = Metadata_Item::DYNAMIC_ARRAY;

                // element_runtime_size = array_sizeof_my_elements(&derived_ref_value);
                // printf("Size of my element is: %ld\n", element_runtime_size);

                assert(0);
            }
            else if (contains(name_of_type, static_array_name))
            {
                info = Metadata_Item::STATIC_ARRAY;

                // @Fixme: fix Static_Array first!!!
                // element_runtime_size = array_sizeof_my_elements(&derived_ref_value);
                // printf("Size of my element is: %ld\n", element_runtime_size);

                assert(0);
            }
            else
            {
                printf("Tagging Metadata_Item::UNKNOWN as the type of the field for '%s'!\n", derived_field);
                assert(0);
            }
        }

        // @Hack: cleanup
        bool is_leaf = true;
        bool old_is_top_level = is_top_level;

        if (cmp_var_type_to_type(probe_type, Entity*))
        {
            // printf("Skipping the entity base!\n");
            is_top_level = false;
            is_leaf = false;
        }
        else if (cmp_var_type_to_type(probe_type, Vector2))
        {
            Vector2 v2_instance;
            fill_metadata(&v2_instance, m, from_base_entity, actual_byte_offset, false);

            is_leaf = false;
        }
        else if (cmp_var_type_to_type(probe_type, Vector3))
        {
            Vector3 v3_instance;
            fill_metadata(&v3_instance, m, from_base_entity, actual_byte_offset, false);

            is_leaf = false;
        }
        else if (cmp_var_type_to_type(probe_type, Vector4))
        {
            Vector4 v4_instance;
            fill_metadata(&v4_instance, m, from_base_entity, actual_byte_offset, false);

            is_leaf = false;
        }
        else if (cmp_var_type_to_type(probe_type, Quaternion))
        {
            Quaternion quat_instance;
            fill_metadata(&quat_instance, m, from_base_entity, actual_byte_offset, false);

            is_leaf = false;
        }

        // printf("[field]: #%ld, '%s', offsets '%ld', from base? %d\tsizeof = %ld\n",
        //        m->leaf_items.count, derived_field, actual_byte_offset,
        //        from_base_entity, sizeof(actual_value));

        auto m_item = New<Metadata_Item>(false);

        // Init the metadata item
        m_item->metadata = m;
        // @Important: Apparently we don't need to do copy string here. So don't free the name.
        m_item->name = String(derived_field); // copy_string(String(derived_field));
        m_item->byte_offset = actual_byte_offset;
        m_item->info = info;

        m_item->runtime_size = sizeof(actual_value);
        // m_item->element_runtime_size = element_runtime_size;

        m_item->imported_from_base_entity = from_base_entity;

        if (is_leaf)
        {
            // printf("Adding item '%s' to list of leaf items (%ld bytes)!\n", derived_field, m_item->runtime_size);

            // Add the item to the list of derived items in the metadata
            array_add(&m->leaf_items, m_item);
        }

        if (is_top_level)
        {
            // printf("Adding item '%s' to list of top level items (%ld bytes)!\n", derived_field, m_item->runtime_size);

            // Add the item to the list of top level items in the metadata
            array_add(&m->top_level_items, m_item);
        }

        // @Hack:
        is_top_level = old_is_top_level;
    });
}

inline
void init_entity_metadata(Metadata *m, _type_Type entity_type, bool is_base_entity)
{
    m->entity_type = entity_type;

    array_init(&m->leaf_items);
    array_init(&m->top_level_items);
}

// @Important: Be careful not to add the same type twice!!
template <typename T>
void Make_Metadata()
{
    auto probe_type = _make_Type(T);
    bool is_base_entity = cmp_var_type_to_type(probe_type, Entity);

    assert(!is_base_entity);

    auto m = New<Metadata>(false);
    init_entity_metadata(m, probe_type, is_base_entity);

    array_add(&metadata_per_entity_type, m);

    T entity_type_instance;
    fill_metadata(&entity_type_instance, m, false, 0, true);

    auto size = static_cast<i64>(sizeof(T));
    // printf("Size of type '%s' is %ld\n", probe_type.name(), size);
    table_add(&runtime_size_per_entity_type, probe_type, size);
}

i64 get_runtime_size_of_entity(_type_Type entity_type)
{
    auto [size, found] = table_find(&runtime_size_per_entity_type, entity_type);

    if (!found)
    {
        printf("[metadata]: Could not find runtime size for entity '%s'\n", entity_type.name());
        assert(0);
        return -1;
    }

    return size;
}

void init_metadata_for_all_entities_type()
{
    init(&runtime_size_per_entity_type);

    // Metadata for the base entity:
    {
        // @Cutnpaste from Make_Metadata:
        auto base_type = _make_Type(Entity);
        init_entity_metadata(&base_entity_metadata, base_type, true);
        Entity base_entity_instance;
        fill_metadata(&base_entity_instance, &base_entity_metadata, true, 0, true);

        auto size = static_cast<i64>(sizeof(Entity));
        table_add(&runtime_size_per_entity_type, base_type, size);
    }

    Make_Metadata<Guy>();
    Make_Metadata<Door>();
    Make_Metadata<Gate>();
    Make_Metadata<Switch>();
    Make_Metadata<Rock>();
    Make_Metadata<Wall>();
    Make_Metadata<Floor>();
}
