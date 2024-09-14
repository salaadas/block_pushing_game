#pragma once

#include "common.h"

// @Hardcoded:
constexpr i64 BUILDER_BUFFER_SIZE = 16384;

struct String_Builder;
void init_string_builder(String_Builder *builder);
void free_buffers(String_Builder *builder);

struct String_Builder
{
    struct Buffer
    {
        SArr<u8> data;
        // This is the producer cursor, it does not decrease if you consume bytes.
        i64 occupied = 0;
        i64 consumer_cursor = 0;
        Buffer *next = NULL;
    };

    // We won't dynamically allocate if we only need one buffer's worth of data.
    Buffer base_buffer;

    // Currently, you have to call init() just to set current buffer to point to
    // base_buffer. So we have the constructor to do that for us.
    Buffer *current_buffer = NULL;

    Allocator allocator;

    // If we ever fail to allocate enough memory, this gets set to true...
    // we can't be sure if the entire string has been added or not.
    bool failed = false;

    // Consumer stuff:
    Buffer *current_consumer_buffer = NULL;

    // My only exception for using constructor and destructor
    String_Builder() {init_string_builder(this);}
    ~String_Builder() {free_buffers(this);}
};

void print_bytes(u8 *s, i64 count);
void reset(String_Builder *builder);
// bool expand(String_Builder *builder);
bool ensure_contiguous_space(String_Builder *builder, i64 bytes);
void append(String_Builder *builder, u8 *s, i64 length, bool backwards = false);
void append(String_Builder *builder, String s);
i64 builder_string_length(String_Builder *builder);
String builder_to_string(String_Builder *builder, Allocator allocator = {});

// Write to stdout for now
i64 write_builder(String_Builder *builder);
