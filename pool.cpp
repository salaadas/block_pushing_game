#include "pool.h"

// @Important: Global context allocator needs to be defined!!!
void set_allocators(Pool     *pool,
                    Allocator block_allocator,
                    void     *block_allocator_data,
                    Allocator array_allocator,
                    void     *array_allocator_data)
{
    if (!block_allocator)
    {
        block_allocator      = global_context.allocator;       // Uses global_context's allocator
        block_allocator_data = global_context.allocator.data;
    }

    if (!array_allocator)
    {
        array_allocator      = global_context.allocator;       // Uses global_context's allocator
        array_allocator_data = global_context.allocator.data;
    }

    pool->block_allocator      = block_allocator;
    pool->block_allocator_data = block_allocator_data;

    pool->   unused_memblocks.allocator = array_allocator;
    pool->     used_memblocks.allocator = array_allocator;
    pool->obsoleted_memblocks.allocator = array_allocator;

    pool->   unused_memblocks.allocator.data = array_allocator_data;
    pool->     used_memblocks.allocator.data = array_allocator_data;
    pool->obsoleted_memblocks.allocator.data = array_allocator_data;
}

// Forward declare
void cycle_new_block(Pool *pool);
void ensure_memory_exists(Pool *pool, i64 nbytes);

void *get(Pool *pool, i64 nbytes)
{
    nbytes = (nbytes + 7) & ~7;
    if (pool->bytes_left < nbytes) ensure_memory_exists(pool, nbytes);

    auto retval = pool->current_pos;
    pool->current_pos += nbytes;
    pool->bytes_left -= nbytes;

    return retval;
}

void release(Pool *pool)
{
    if (!pool->has_used_memory) return;
    pool->has_used_memory = false;

    reset(pool);

    for (auto it : pool->unused_memblocks) my_free(it);
    array_free(&pool->unused_memblocks);
}

void reset(Pool *pool)
{
    if (pool->current_memblock)
    {
        array_add(&pool->unused_memblocks, pool->current_memblock);
        pool->current_memblock = NULL;
    }

    for (auto it : pool->used_memblocks) array_add(&pool->unused_memblocks, it);
    array_free(&pool->used_memblocks);

    for (auto it : pool->obsoleted_memblocks) my_free(it);
    array_free(&pool->obsoleted_memblocks);

    cycle_new_block(pool);
}

//
// Stuff below is private for Pool
//
void resize_blocks(Pool *pool, i64 block_size);

void ensure_memory_exists(Pool *pool, i64 nbytes)
{
    pool->has_used_memory = true;

    auto bs = pool->memblock_size; // block size

    while (bs < nbytes) bs *= 2;

    // if anything changes at all
    if (bs > pool->memblock_size) resize_blocks(pool, bs);

    cycle_new_block(pool);
}

void resize_blocks(Pool *pool, i64 block_size)
{
    pool->memblock_size = block_size;

    if (pool->current_memblock) array_add(&pool->obsoleted_memblocks, pool->current_memblock);

    for (auto it : pool->used_memblocks) array_add(&pool->obsoleted_memblocks, it);
    pool->used_memblocks.count = 0;

    pool->current_memblock = NULL;
}

void cycle_new_block(Pool *pool)
{
    if (pool->current_memblock)
    {
        array_add(&pool->used_memblocks, pool->current_memblock);
        pool->current_memblock = NULL;
    }

    u8 *new_block = NULL;
    if (pool->unused_memblocks.count)
    {
        new_block = pop(&pool->unused_memblocks);
    }
    else
    {
        assert(pool->block_allocator);
        new_block = (u8*)pool->block_allocator.proc(Allocator_Mode::ALLOCATE,
                                                    pool->memblock_size, 0,
                                                    NULL, (void*)pool->block_allocator_data);
    }

    pool->bytes_left       = pool->memblock_size;
    pool->current_pos      = new_block;
    pool->current_memblock = new_block;
}


// ----------------------------------------------------------------------


void *pool_allocator(Allocator_Mode mode, i64 size, i64 old_size,
                     void *old_memory_pointer, void *allocator_data)
{
    auto pool = static_cast<Pool*>(allocator_data);
    assert(pool != NULL);

    // printf("In pool_allocator, mode %d, size %ld, data %p\n",
    //        static_cast<i32>(mode), size, allocator_data);

    if (mode == Allocator_Mode::ALLOCATE)
    {
        return get(pool, size);
    }
    else if (mode == Allocator_Mode::RESIZE)
    {
        // Pools don't expand allocated things. It gets new memory and copy.
        // If we want to get sophisticated we can let the Pool expand the last
        // entry in any bucket, but it is unclear how useful this is.

        auto result = get(pool, size);
        if (!result) return NULL;

        if (old_memory_pointer && (old_size > 0))
            memcpy(result, old_memory_pointer, old_size);

        return result;
    }
    else if (mode == Allocator_Mode::FREE)
    {
        // Pools don't free individual elements
        return NULL;
    }
    else if (mode == Allocator_Mode::FREE_ALL)
    {
        reset(pool);
        return NULL;
    }

    assert(0); // Should not get here!
}
