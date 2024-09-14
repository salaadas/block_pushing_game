/*
  The Pool is a memory allocator that you can use when you want to
  quickly allocate memory blocks of many different sizes, but all
  those blocks will have approximately the same lifetime.
  With a Pool, you allocate and allocate but you never deallocate
  until you are done with everything; at that time, you deallocate
  the entire Pool at once.
*/

#pragma once

#include "common.h"
#include "array.h"

struct Pool
{
    static const i64 POOL_BUCKET_SIZE_DEFAULT = 65536;
    i64              memblock_size = POOL_BUCKET_SIZE_DEFAULT;
    i32              alignment = 8;

    RArr<u8*>        unused_memblocks;
    RArr<u8*>        used_memblocks;
    RArr<u8*>        obsoleted_memblocks;

    u8              *current_memblock = NULL;
    u8              *current_pos      = NULL;
    i64              bytes_left       = 0;

    Allocator        block_allocator;
    void            *block_allocator_data;

    bool             has_used_memory = false;
};

// will be an overloaded function once we create multiple allocators
void set_allocators(Pool     *pool,
                    Allocator block_allocator      = {},
                    void     *block_allocator_data = NULL,
                    Allocator array_allocator      = {},
                    void     *array_allocator_data = NULL);
void *get    (Pool *pool, i64 nbytes);
void  release(Pool *pool);
void  reset  (Pool *pool);

void *pool_allocator(Allocator_Mode mode, i64 size, i64 old_size,
                     void *old_memory_pointer, void *allocator_data);
