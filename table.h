#pragma once

#include "common.h"
#include "hash.h"

#define COUNT_COLLISION
#define NEVER_OCCUPIED_HASH 0
#define REMOVED_HASH        1
#define FIRST_VALID_HASH    2

template <typename K, typename V>
struct __Table_Entry
{
    u32 hash = 0;
    K   key;
    V   value;
};

// Table that maps keys to values using 32 bit hashes
template <typename K, typename V>
struct Table
{
    // using Hash_Function = std::function<u32(K)>;
    // using Cmp_Function  = std::function<bool(K, K)>;

    typedef u32(*Hash_Function)(K);
    typedef bool(*Cmp_Function)(K, K);

    // @Cleanup: clean up this get_hash and equal mess
    Hash_Function         hash_function = get_hash;
    Cmp_Function          cmp_function  = equal;

    Table(Cmp_Function cfn = NULL, Hash_Function hfn = NULL, u32 load_factor = 70)
        : LOAD_FACTOR_PERCENT(load_factor)
    {
        // assert(hash_function);
        // assert(cmp_function);
        if (!hash_function) hash_function = get_hash;
        if (!cmp_function)  cmp_function  = equal;
    }

    i64                         count        = 0; // number of valid items in the table
    i64                         allocated    = 0; // number of slots for which we have allocated
    i64                         slots_filled = 0; // the number of slots that can't be occupy
    u32                         LOAD_FACTOR_PERCENT;
    Allocator                   allocator;
    __Table_Entry<K, V>        *entries;
#ifdef COUNT_COLLISION
    i64                         add_collisions  = 0;
    i64                         find_collisions = 0;
#endif
    static const i64            SIZE_MIN = 32;


    // Table helpers below
    struct __Table_Iterator
    {
        Table<K, V> *owner;
        i64          entries_index;

        __Table_Entry<K, V> *operator->() {return &owner->entries[entries_index];}
        __Table_Entry<K, V> &operator*()  {return owner->entries[entries_index];}

        __Table_Iterator &operator++()
        {
            while (true)
            {
                entries_index += 1;
                if (entries_index == owner->allocated) break;
                if (owner->entries[entries_index].hash >= FIRST_VALID_HASH) break;
            }

            return *this;
        }

        bool operator==(__Table_Iterator rhs) {return entries_index == rhs.entries_index;}
        bool operator!=(__Table_Iterator rhs) {return entries_index != rhs.entries_index;}
    };

    __Table_Iterator begin()
    {
        __Table_Iterator it;
        it.owner = this;
        // The -1 is here so that when we try to obtain the first valid entry, 
        // we need to increase the entries_index first and then check.
        // So if we put entries_index as 0, we would then check for the entry at
        // index 1; thus, missing the first one.
        it.entries_index = -1;
        ++it;
        return it;
    }

    __Table_Iterator end()
    {
        __Table_Iterator it;
        it.owner = this;
        it.entries_index = allocated;
        return it;
    };
};

template <typename K, typename V>
void default_table(Table<K, V> *table)
{
    table->hash_function = get_hash;
    table->cmp_function  = equal;
    table->LOAD_FACTOR_PERCENT = 70;
    table->allocator = (Allocator){};
}

inline
i32 __table_next_power_of_two(i32 x)
{
    assert(x != 0);
    auto p = 1;
    while (x > p) p += p;
    return p;
}

template <typename K, typename V>
void resize(Table<K, V> *table, i64 slots_to_allocate = 0)
{
    if (slots_to_allocate == 0) slots_to_allocate = Table<K, V>::SIZE_MIN;

    auto n = __table_next_power_of_two(slots_to_allocate);

    table->allocated = n;
    table->entries   = (__Table_Entry<K, V>*)(my_alloc(sizeof(__Table_Entry<K, V>) * n, table->allocator));

    for (i32 i = 0; i < n; ++i)
        table->entries[i].hash = NEVER_OCCUPIED_HASH;
}

template <typename K, typename V>
void init(Table<K, V> *table, i64 slots_to_allocate = 0)
{
    default_table(table);

    assert(table->hash_function);
    assert(table->cmp_function);

    if (!table->allocator)
    {
        table->allocator = global_context.allocator;
        if (!table->allocator) table->allocator = {NULL, __default_allocator};
    }

    resize(table, slots_to_allocate);
}

template <typename K, typename V>
void deinit(Table<K, V> *table)
{
    my_free(table->entries, table->allocator);
}

template <typename K, typename V>
void expand(Table<K, V> *table)
{
    auto old_entries   = table->entries;
    auto old_allocated = table->allocated;

    // If we were adding and removing lots of stuff from the table,
    // we might have lots of slots filled with REMOVED_HASH, so,
    // in that case, don't grow.
    i64 new_allocated = 0;

    // The * 2 is to say, if we double the size, are we still small enough to fit into
    // the current memory? The reason we are doing this test is, if we removed a bunch
    // of elements, maybe we are full of REMOVED_HASH markers, and if we just rehash
    // to the same size, we can get rid of those. An alternate version (simpler?) might
    // be to check table->count vs table->slots_filled.
    if ((table->count * 2 + 1) * 100 < (table->allocated * table->LOAD_FACTOR_PERCENT))
    {
        // Just go with the current size, but clean out the removals
        new_allocated = table->allocated;
    }
    else
    {
        // printf("Expanding, literally, count %ld, slots_filled %ld, allocated %ld\n",
        //        table->count , table->slots_filled, table->allocated);

        // Else, we double the current table
        new_allocated = table->allocated * 2;
    }

    if (new_allocated < Table<K, V>::SIZE_MIN) new_allocated = Table<K, V>::SIZE_MIN;

    resize(table, new_allocated);

    table->count        = 0;
    table->slots_filled = 0;

    for (i64 i = 0; i < old_allocated; ++i)
    {
        if (old_entries[i].hash >= FIRST_VALID_HASH)
            table_add(table, old_entries[i].key, old_entries[i].value);
    }

    if (old_allocated) my_free(old_entries, table->allocator);
}

template <typename K, typename V>
V *table_add(Table<K, V> *table, K key, V value)
{
    // A 100% full table will infinite loop (and, you will want to be substantially smaller than this
    // for reasonable perfomance).
    assert(table->LOAD_FACTOR_PERCENT < 100);

    // The + 1 here is to handle the weird case when the table size is 1 and you add the first item...
    // If we just do table_count * 2 >= table.allocated, we would fill the table, causing an infinite loop on find.
    if ((table->slots_filled + 1) * 100 >= table->allocated * table->LOAD_FACTOR_PERCENT)
        expand(table);

    assert(table->slots_filled <= table->allocated);

    bool slot_reused = false;

    // walk code
    auto mask           = (u32)(table->allocated - 1);
    auto hash           = table->hash_function(key);
    if (hash < FIRST_VALID_HASH) hash += FIRST_VALID_HASH;
    auto index          = hash & mask; // same as hash % table->allocated [less clock-cycle]
    u32 probe_increment = 1;

    while (table->entries[index].hash)
    {
        if (table->entries[index].hash == REMOVED_HASH)
        {
            slot_reused = true;
            break;
        }

#ifdef COUNT_COLLISION
        table->add_collisions += 1;
#endif

        index = (index + probe_increment) & mask; // same as (index + probe_increment) % table->allocated
        probe_increment += 1;
    }

    table->count += 1;
    if (!slot_reused)
    {
        table->slots_filled += 1;
    }

    auto entry   = &table->entries[index];
    entry->hash  = hash;
    entry->key   = key;
    entry->value = value;

    return &entry->value;
}

template <typename K, typename V>
void table_reset(Table<K, V> *table)
{
    table->count        = 0;
    table->slots_filled = 0;

    for (i64 i = 0; i < table->allocated; ++i)
        table->entries[i].hash = 0;
}

template <typename K, typename V>
V *table_find_pointer(Table<K, V> *table, K key)
{
    if (!table->allocated) return NULL;

    auto mask           = (u32)(table->allocated - 1);
    auto hash           = table->hash_function(key);
    if (hash < FIRST_VALID_HASH) hash += FIRST_VALID_HASH;
    auto index          = hash & mask;
    u32 probe_increment = 1;

    // walk code
    while (table->entries[index].hash)
    {
        auto entry = &table->entries[index];
        if (entry->hash == hash)
        {
            if (table->cmp_function(entry->key, key)) return &entry->value;
        }

#ifdef COUNT_COLLISION
        table->find_collisions += 1;
#endif

        index = (index + probe_increment) & mask;
        probe_increment += 1;
    }

    return NULL;
}

// This is kind of like table_set, but used when you
// just want a pointer to the value, which you can fill in.
template <typename K, typename V>
V *find_or_add(Table<K, V> *table, K key)
{
    auto value = table_find_pointer(table, key);
    if (value) return value;

    V new_value = {};
    value = table_add(table, key, new_value);
    return value;
}

template <typename K, typename V>
V *table_set(Table<K, V> *table, K key, V value)
{
    auto value_ptr = table_find_pointer(table, key);
    if (value_ptr)
    {
        *value_ptr = value;
        return value_ptr;
    }
    else
    {
        return table_add(table, key, value);
    }
}

template <typename K, typename V>
my_pair<V, bool> table_find(Table<K, V> *table, K key)
{
    auto pointer = table_find_pointer(table, key);
    if (pointer) return {*pointer, true};

    V dummy;
    return {dummy, false};
}

template <typename K, typename V>
RArr<V> table_find_multiple(Table<K, V> *table, K key, Allocator allocator = {})
{
    RArr<V> results;

    if (!table->allocated) return results;

    if (allocator)
    {
        results.allocator = allocator;
    }
    else
    {
        results.allocator = table->allocator;
    }

    // walk code
    auto mask           = (u32)(table->allocated - 1);
    auto hash           = table->hash_function(key);
    if (hash < FIRST_VALID_HASH) hash += FIRST_VALID_HASH;
    auto index          = hash & mask;
    u32 probe_increment = 1;

    while (table->entries[index].hash)
    {
        auto entry = &table->entries[index];
        if (entry->hash == hash)
        {
            if (table->cmp_function(entry->key, key))
            {
                array_add(&results, entry->value);
            }
            else
            {
#ifdef COUNT_COLLISION
                table->find_collisions += 1;
#endif
            }
        }
        else
        {
#ifdef COUNT_COLLISION
            table->find_collisions += 1;
#endif
        }

        index = (index + probe_increment) & mask;
        probe_increment += 1;
    }

    return results;
}

// Remove the first entry at the given key. Returns false if the key was not found.
template <typename K, typename V>
my_pair<bool, V> table_remove(Table<K, V> *table, K key)
{
    if (!table->allocated)
    {
        V dummy;
        return {false, dummy};
    }

    // walk code
    auto mask           = (u32)(table->allocated - 1);
    auto hash           = table->hash_function(key);
    if (hash < FIRST_VALID_HASH) hash += FIRST_VALID_HASH;
    auto index          = hash & mask; // same as hash % table->allocated [less clock-cycle]
    u32 probe_increment = 1;

    while (table->entries[index].hash)
    {
        auto entry = &table->entries[index];

        if ((entry->hash == hash) && table->cmp_function(entry->key, key))
        {
            entry->hash = REMOVED_HASH;
            table->count -= 1;
            return {true, entry->value};
        }

#ifdef COUNT_COLLISION
        table->find_collisions += 1;
#endif

        index = (index + probe_increment) & mask;
        probe_increment += 1;
    }

    V dummy;
    return {false, dummy};
}
