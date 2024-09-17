#pragma once

#include "common.h"

//
// Resizable array/dynamic array starts here
// We chose to make Resizable_Array allocate according to the allocator.
// So they could be allocated on the heap/pool/...
//

template <typename Value_Type>
struct Resizable_Array
{
    i64         allocated = 0;
    i64         count     = 0;
    Value_Type *data      = NULL;
    Allocator   allocator = {};

    //
    // Helper functions below this line
    //

    Value_Type *begin() { if ((data == NULL) || (count <= 0)) return NULL; return &data[0]; }
    Value_Type *end()   { if ((data == NULL) || (count <= 0)) return NULL; return &data[count]; }
    Value_Type &operator[](i64 index)
    {
        assert(index < count);
        assert(index >= 0);
        assert(data != NULL);

        return data[index];
    }

    operator bool() { return count > 0; }

    Resizable_Array<Value_Type> operator=(std::initializer_list<Value_Type> rhs)
    {
        array_reset(this);
        array_reserve(this, rhs.size());

        i64 it = 0;
        for (auto val : rhs)
        {
            array_add(this, val);
            it += 1;
        }

        return *this;
    }
};

template <typename V>
using RArr = Resizable_Array<V>;

template <typename V>
void array_init(RArr<V> *array)
{
    array->count     = 0;
    array->allocated = 0;
    array->allocator = (Allocator){};
}

template <typename V>
void array_add(RArr<V> *array, V value)
{
    if (array == NULL) return;

    if (array->count >= array->allocated)
    {
        i64 reserve = array->allocated * 2;
        if (reserve < 8) reserve = 8;

        array_reserve(array, reserve);
    }

    array->data[array->count] = value;
    array->count += 1;
}

template <typename V>
void array_reset(RArr<V> *array)
{
    if (array == NULL) return;

    // @Note: We shouldn't reallocate the array, right?
    array->count = 0; // set the count to 0, so that new elements override old one
}

template <typename V>
V *array_find(RArr<V> *array, V value)
{
    if (array == NULL) return NULL;

    auto it = array->begin();
    auto i = 0;
    while (i < array->count)
    {
        if (*it == value) return it;
        i += 1;
        it += 1;
    }

    return NULL;
}

template <typename V>
void array_copy(RArr<V> *dest, RArr<V> *src)
{
    // @Todo: array_copy is not yet implemented
    assert(0);
}

template <typename V>
void array_reserve(RArr<V> *array, i64 size)
{
    if ((array == NULL) || (size <= 0) || (size <= array->allocated)) return;
    // We make sure that the count does not get modified so that it is larger than 
    // the allocated prior to the reservation.
    // @Note: Change the count to 0 if you don't care about the old value inside the array.
    // Otherwise, any value of count that is greater than 0 will make the array do RESIZE.
    assert(array->count <= array->allocated);

    // @Important: We try to use the allocator inside the array.
    // If there isn't one, we use the global context allocator.
    // Then if that is still unavailable, we then try to use the heap allocator.
    if (!array->allocator)
    {
        array->allocator = global_context.allocator;
        if (!array->allocator) array->allocator = {NULL, __default_allocator};
    }

    // If there is something already inside the array,
    // we do a RESIZE instead of ALLOCATE
    Allocator_Mode mode;
    if (array->count > 0) mode = Allocator_Mode::RESIZE;
    else                  mode = Allocator_Mode::ALLOCATE;

    auto new_memory = static_cast<V*>(array->allocator.proc(mode,
                                                            size             * sizeof(V),
                                                            array->allocated * sizeof(V),
                                                            array->data, array->allocator.data));
    assert((new_memory));

    array->data      = new_memory;
    array->allocated = size;
}

template <typename V>
void array_resize(RArr<V> *array, i64 size)
{
    array_reserve(array, size);
    array->count = size;
}

template <typename V>
void array_free(RArr<V> *array)
{
    if (!array->data || (array->count <= 0)) return;

    array->allocator.proc(Allocator_Mode::FREE, 0, 0,
                          array->data, array->allocator.data);
    array->data      = NULL;
    array->count     = 0;
    array->allocated = 0;
}

template <typename V>
inline
void my_free(RArr<V> *array)
{
    array_free(array);
}

template <typename T>
void swap_elements(T *a, T *b)
{
    auto temp = *a;
    *a = *b;
    *b = temp;
}

template <typename T>
void array_ordered_remove_by_index(RArr<T> *array, i64 index)
{
    assert((array->count != 0));
    array->count -= 1;

    auto size_of_one_element = sizeof(T);

    u8 *dest = reinterpret_cast<u8*>(array->data) + index * size_of_one_element;
    u8 *src  = dest + size_of_one_element;

    auto n = array->count - index;
    if (!n) return;

    array->data = reinterpret_cast<T*>(memcpy(dest, src, n * size_of_one_element));
}

// Swap the item to remove with the last element
// Then shrink the array by 1, so that the item is effectively removed
template <typename V>
bool array_unordered_remove_by_value(RArr<V> *array, V value)
{
    for (i64 it = 0; it < array->count; ++it)
    {
        if ((*array)[it] == value)
        {
            (*array)[it] = (*array)[array->count - 1];
            array->count -= 1;

            return true;
        }
    }

    return false;
}

template <typename T>
T pop(RArr<T> *array)
{
    if (array->count == 0)
    {
        assert(0);
    }

    auto result = array->data[array->count - 1];
    array->count -= 1;

    return result;
}

template <typename T>
using Array_Sort_Func = bool(*)(T a, T b);

template <typename T>
void array_actual_qsort(RArr<T> *array, Array_Sort_Func<T> sort_function, i64 lo, i64 hi)
{
    if (lo >= hi || lo < 0) return;

    //
    // Partition the array and get the pivot index
    //
    i64 pivot;
    {
        auto p = array->data[hi];
        auto i = lo;

        for (i64 j = lo; j < hi; ++j)
        {
            if (sort_function(array->data[j], p))
            {
                swap_elements(&array->data[i], &array->data[j]);

                i += 1;
            }
        }

        swap_elements(&array->data[i], &array->data[hi]);

        pivot = i;
    }

    //
    // Sort the two partitions
    //
    array_actual_qsort(array, sort_function,        lo, pivot - 1);
    array_actual_qsort(array, sort_function, pivot + 1,        hi);
}

template <typename T>
void array_add_if_unique(RArr<T> *array, T x)
{
    auto found = array_find(array, x);

    if (found != NULL) return;

    array_add(array, x);
}

template <typename T>
void array_qsort(RArr<T> *array, Array_Sort_Func<T> sort_function)
{
    array_actual_qsort(array, sort_function, 0, array->count - 1);
}

//
// Static array starts here
// These are arrays that are allocated on the stacks.
// Use them for small data storage.
// 

template <typename V>
struct Static_Array;

template <typename V>
Static_Array<V> NewArray(i64 count, i64 alignment = 8, Allocator allocator = {}); // @ForwardDeclare

template <typename Value_Type>
struct Static_Array
{
    i64         count = 0;
    Value_Type  *data = NULL;

    Allocator allocator;

    explicit Static_Array() {}
    explicit Static_Array(i64 c)
    {
        *this = NewArray<Value_Type>(c);
    }

    ~Static_Array()
    {
        // This does not free the individual elements if the elements are allocated seperately.
        // @Leak if you don't call array_free after exiting the scope.
        // array_free(this);
    }

    //
    // Helper functions below this line
    //

    void set(Value_Type rhs[])
    {
        memcpy(data, rhs, sizeof(data));
    }
    Static_Array &operator=(Value_Type rhs[]) { set(rhs); return *this; }

    void clear()
    {
        memset(data, 0, sizeof(Value_Type) * count);
    }

    // @Todo: missing copy procedure...

    Value_Type *begin() { if ((data == NULL) || (count <= 0)) return NULL; return &data[0]; }
    Value_Type *end()   { if ((data == NULL) || (count <= 0)) return NULL; return &data[count]; }
    Value_Type &operator[](i64 index)
    {
        assert(index < count);
        assert(index >= 0);
        assert(data != NULL);

        return data[index];
    }

    Static_Array<Value_Type> operator=(std::initializer_list<Value_Type> rhs)
    {
        i64 it = 0;
        for (auto val : rhs)
        {
            if (it == count) break;

            data[it] = val;

            it += 1;
        }

        return *this;
    }

    operator bool() { return count > 0; }
};

template <typename V>
using SArr = Static_Array<V>;

template <typename V>
SArr<V> NewArray(i64 count, i64 alignment, Allocator allocator)
{
    SArr<V> result;

    result.allocator = allocator;
    if (!result.allocator) result.allocator = global_context.allocator;
    if (!result.allocator) result.allocator = {NULL, __default_allocator};

    // @Note: We are disabling the array alignment partly because this can lead to unexpected assertions.
    // auto alignment_extra_bytes = count % alignment; // This does not work if alignment is 1
    // result.count = count + alignment - alignment_extra_bytes;
    result.count = count;

    result.data = reinterpret_cast<V*>(my_alloc(sizeof(V) * result.count, result.allocator));
    result.clear();

    return result;
}

template <typename V>
void array_free(SArr<V> *array)
{
    if (!array->data || (array->count <= 0)) return;

    array->allocator.proc(Allocator_Mode::FREE, 0, 0,
                          array->data, array->allocator.data);
    array->data      = NULL;
    array->count     = 0;
}

template <typename V>
inline
void my_free(SArr<V> *array)
{
    array_free(array);
}
