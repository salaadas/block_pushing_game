#pragma once

#include "common.h"

// With bucket arrays, we have the advantages of:
// - Avoid copying items
// - Items don't move in memory, so you can take pointers to them.
//   => Pointers stability

// Main procedures:
// my_pair<Bucket_Locator, T*> bucket_array_add(Bucket_Array<T, items_max> *array, T item)
// void                        bucket_array_remove(Bucket_Array<T, items_max> *array, Bucket_Locator locator)
// T                           bucket_array_find(Bucket_Array<T, items_max> *array, Bucket_Locator locator)
// void                        bucket_array_reset(Bucket_Array<T, items_max> *array)


template <typename Type, i32 items_max>
struct Bucket
{
    SArr<bool> occupied; // @Important: Remember to clear this (contains garbage value)
    SArr<Type> data;
    // @Important: check if the default initialized value for data[..]

    // Index that *may* not be occupied, to decrease the search space when adding elements
    i32                   lowest_maybe_not_occupied = 0;
    u32                   bucket_index = 0;
    i32                   count = 0; // Items that are allocated inside this particular bucket
};

struct Bucket_Locator
{
    u32 bucket_index = 0;
    i32 slot_index   = 0; // Signed because we do a trick where we set it to -1
};

template <typename Type, i32 items_per_bucket>
struct Bucket_Array
{
    i64 count = 0; // Items that are allocated across the entire array

    Allocator allocator;

    using My_Bucket = Bucket<Type, items_per_bucket>;

    RArr<My_Bucket*> all_buckets;
    RArr<My_Bucket*> unfull_buckets;


    struct __Bucket_Array_Iterator
    {
        Bucket_Locator locator;
        Bucket_Array<Type, items_per_bucket> *owner;

        Type *operator->()
        {
            auto bucket = owner->all_buckets[locator.bucket_index];
            return &bucket->data[locator.slot_index];
        }
        Type &operator*()
        {
            auto bucket = owner->all_buckets[locator.bucket_index];
            return bucket->data[locator.slot_index];
        }
        __Bucket_Array_Iterator &operator++()
        {
            // Set the locator to the next occupied value
            // if there are no occupied values left in the bucket,
            // return the end of the last bucket

            while (true)
            {
                locator.slot_index += 1;

                if (locator.slot_index == items_per_bucket &&
                    locator.bucket_index == (owner->all_buckets.count - 1))
                {
                    break;
                }

                if (locator.slot_index == items_per_bucket)
                {
                    locator.slot_index = 0;
                    locator.bucket_index += 1;
                }

                auto bucket = owner->all_buckets[locator.bucket_index];
                if (bucket->occupied[locator.slot_index])
                {
                    break;
                }
            }

            return *this;
        }
        bool operator==(__Bucket_Array_Iterator rhs)
        {
            return (locator.bucket_index == rhs.locator.bucket_index) &&
                (locator.slot_index == rhs.locator.slot_index);
        }
        bool operator!=(__Bucket_Array_Iterator rhs)
        {
            return (locator.bucket_index != rhs.locator.bucket_index) ||
                (locator.slot_index != rhs.locator.slot_index);
        }
    };
    __Bucket_Array_Iterator begin()
    {
        __Bucket_Array_Iterator it;
        it.locator.bucket_index = 0;

        // The -1 is here so that we can probe for the first valid entry in the bucket array.
        // If we were to set it as 0 and probe for the first valid entry, it would be tremendously
        // harder and creates for branches in the code.
        it.locator.slot_index   = -1;
        it.owner                = this;

        ++it;
        return it;
    };
    __Bucket_Array_Iterator end()
    {
        __Bucket_Array_Iterator it;
        it.locator.bucket_index = all_buckets.count - 1;
        it.locator.slot_index   = items_per_bucket;
        it.owner                = this;
        return it;
    };
};

template <typename T, i32 items_max>
void bucket_array_init(Bucket_Array<T, items_max> *array)
{
    array->count = 0;
    array->allocator = {};
    array_init(&array->all_buckets);
    array_init(&array->unfull_buckets);
}

template <typename T, i32 items_max>
Bucket<T, items_max> *add_bucket(Bucket_Array<T, items_max> *array)
{
    assert(array->unfull_buckets.count == 0);

    // This is the first call, that's why the size is 0
    if (!array->all_buckets.count)
    {
        if (array->allocator)
        {
            array->all_buckets.allocator    = array->allocator;
            array->unfull_buckets.allocator = array->allocator;
        }
    }

    Context preserved_context = global_context;

    if (array->allocator) global_context.allocator = array->allocator;
    
    Bucket<T, items_max> *new_bucket = New<Bucket<T, items_max>>(false);
    new_bucket->bucket_index = static_cast<u32>(array->all_buckets.count);
    assert(new_bucket->bucket_index == array->all_buckets.count);

    // @Important: Setting the count of array here explicitly because New does not initialize values
    new_bucket->occupied = NewArray<bool>(items_max);
    new_bucket->data = NewArray<T>(items_max);

    new_bucket->occupied.clear(); // Clear the occupied array (which contains garbage values) to all false

    // Add the new bucket to both bucket pools using the [custom/global] allocator
    array_add(&array->all_buckets,    new_bucket);
    array_add(&array->unfull_buckets, new_bucket);

    global_context = preserved_context;

    return new_bucket;
}

template <typename T, i32 items_max>
my_pair<T*, Bucket_Locator> find_and_occupy_empty_slot(Bucket_Array<T, items_max> *array)
{
    // If there is no unfull bucket, make a new one
    if (!array->unfull_buckets.count) add_bucket(array);
    assert(array->unfull_buckets.count > 0);

    auto bucket = array->unfull_buckets[0];
    auto index = -1;

    for (auto it = bucket->lowest_maybe_not_occupied; it < items_max; ++it)
    {
        if (!bucket->occupied[it])
        {
            index = it;
            break;
        }
    }

    assert(index != -1);

    bucket->occupied[index] = true;
    bucket->count += 1;
    bucket->lowest_maybe_not_occupied = static_cast<i32>(index + 1);
    assert(bucket->count <= items_max);

    array->count += 1;

    if (bucket->count == items_max)
    {
        auto removed = array_unordered_remove_by_value(&array->unfull_buckets, bucket);
        assert((removed == true));
    }

    Bucket_Locator locator;
    locator.bucket_index = bucket->bucket_index;
    locator.slot_index   = static_cast<i32>(index);

    auto memory = &bucket->data[index];
    return {memory, locator};
}


// @Important:
// Bucket array functions are below


template <typename T, i32 items_max>
void bucket_array_reset(Bucket_Array<T, items_max> *array)
{
    for (auto it : array->all_buckets)
    {
        my_free(&it->occupied);
        my_free(&it->data);

        my_free(it, array->allocator);
    }
    array_reset(&array->all_buckets);
    array_reset(&array->unfull_buckets);

    array->count = 0;
}

template <typename T, i32 items_max>
my_pair<Bucket_Locator, T*> bucket_array_add(Bucket_Array<T, items_max> *array, T item)
{
    auto [pointer, locator] = find_and_occupy_empty_slot(array);

    *pointer = item;

    return {locator, pointer};
}

template <typename T, i32 items_max>
T bucket_array_find(Bucket_Array<T, items_max> *array, Bucket_Locator locator)
{
    auto bucket = array->all_buckets[locator.bucket_index];
    assert(bucket->occupied[locator.slot_index] == true);

    auto result = bucket->data[locator.slot_index];
    return result;
}

template <typename T, i32 items_max>
void bucket_array_remove(Bucket_Array<T, items_max> *array, Bucket_Locator locator)
{
    auto bucket = array->all_buckets[locator.bucket_index];
    assert(bucket->occupied[locator.slot_index] == true);

    auto was_full = (bucket->count == items_max);

    bucket->occupied[locator.slot_index] = false;

    if (locator.slot_index < bucket->lowest_maybe_not_occupied)
    {
        bucket->lowest_maybe_not_occupied = static_cast<i32>(locator.slot_index);
    }

    bucket->count -= 1;
    array->count  -= 1;

    if (was_full)
    {
        assert(array_find(&array->unfull_buckets, bucket) == NULL);
        array_add(&array->unfull_buckets, bucket);
    }
}
