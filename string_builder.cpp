#include "string_builder.h"

void print_bytes(u8 *s, i64 count)
{
    for (i64 i = 0; i < count; ++i)
    {
        printf("%x ", s[i]);
    }
    newline();
}

void init_string_buffer(String_Builder::Buffer *buffer)
{
    buffer->data.count      = BUILDER_BUFFER_SIZE;
    buffer->occupied        = 0;
    buffer->consumer_cursor = 0;
    buffer->next            = NULL;
}

void init_string_builder(String_Builder *builder)
{
    init_string_buffer(&builder->base_buffer);
    builder->current_buffer = &builder->base_buffer;

    // Ideally we should pick the allocators that are active when the constructor is invoked.
    builder->allocator = global_context.allocator;

    // @Cleanup: Redundant NewArray
    builder->base_buffer.data = NewArray<u8>(BUILDER_BUFFER_SIZE);

    if (!builder->allocator) builder->allocator = {NULL, __default_allocator};
}

void free_buffers(String_Builder *builder)
{
    assert((builder->allocator));

    auto old_allocator = global_context.allocator;
    defer { global_context.allocator = old_allocator; };

    global_context.allocator = builder->allocator;

    auto buffer = builder->base_buffer.next;
    while (buffer)
    {
        auto next = buffer->next;
        my_free(&buffer->data);
        my_free(buffer);
        buffer = next;
    }
}

void reset(String_Builder *builder)
{
    free_buffers(builder);

    auto base_buffer = &builder->base_buffer;
    base_buffer->occupied = 0;
    base_buffer->next = NULL;

    builder->current_buffer = base_buffer;
    builder->current_consumer_buffer = NULL;
}

bool expand(String_Builder *builder)
{
    assert((builder->allocator));

    auto old_allocator = global_context.allocator;
    defer { global_context.allocator = old_allocator; };

    global_context.allocator = builder->allocator;

    auto buffer = New<String_Builder::Buffer>(false);
    init_string_buffer(buffer);
    buffer->data = NewArray<u8>(BUILDER_BUFFER_SIZE);

    if (!buffer) return false;

    builder->current_buffer->next = buffer;
    builder->current_buffer = buffer;
    assert((buffer->next == NULL));

    return true;
}

bool ensure_contiguous_space(String_Builder *builder, i64 bytes)
{
    if (BUILDER_BUFFER_SIZE < bytes) return false; // Can't do it!

    auto available = BUILDER_BUFFER_SIZE - builder->current_buffer->occupied;
    if (available >= bytes) return true;

    return expand(builder);
}

void append(String_Builder *builder, u8 *s, i64 length, bool backwards)
{
    auto length_max = BUILDER_BUFFER_SIZE - builder->current_buffer->occupied;

    if (length_max <= 0)
    {
        auto success = expand(builder);
        if (!success)
        {
            builder->failed = true;
            return;
        }
    }

    auto to_copy = std::min(length, length_max);
    if (length > 0) assert(to_copy >= 0);

    if (backwards)
    {
        // @Speed:
        for (i64 i = 0; i < to_copy; ++i)
        {
            auto j = to_copy - 1 - i;
            builder->current_buffer->data[builder->current_buffer->occupied + i] = s[j];
        }
    }
    else
    {
        memcpy(builder->current_buffer->data.data + builder->current_buffer->occupied, s, to_copy);
    }

    builder->current_buffer->occupied += to_copy;

    if (length > to_copy)
    {
        append(builder, s + to_copy, length - to_copy);
    }
}

void append(String_Builder *builder, String s)
{
    append(builder, s.data, s.count);
}

i64 builder_string_length(String_Builder *builder)
{
    auto buffer = &builder->base_buffer;
    auto bytes  = 0;

    while (buffer)
    {
        bytes += buffer->occupied;
        buffer = buffer->next;
    }

    return bytes;
}

String builder_to_string(String_Builder *builder, Allocator allocator) // Using context allocator
{
    auto count = builder_string_length(builder);

    // @Cleanup:
    if (!allocator) allocator = {NULL, __default_allocator};

    auto result = alloc_string(count, allocator);
    if (!result) return result;

    auto data = result.data;
    auto buffer = &builder->base_buffer;

    while (buffer)
    {
        memcpy(data, buffer->data.data, buffer->occupied);
        data += buffer->occupied;

        buffer = buffer->next;
    }

    return result;
}

// Write to stdout for now
#include <unistd.h>
i64 write_builder(String_Builder *builder)
{
    auto write_buffer = [](String_Builder::Buffer *buffer) -> i64 {
        String s; // for clarity
        s.data = buffer->data.data;
        s.count = buffer->occupied;

        auto written = write(1, s.data, s.count);
        return written;
    };

    auto buffer = &builder->base_buffer;
    i64 written = 0;

    while (buffer)
    {
        written += write_buffer(buffer);
        buffer = buffer->next;
    }

    return written;
}
