/*
  A string is a sequence of characters. Here we represent the string as an array of bytes (u8).
  Our strings are not zero-terminated.
  Currently, we don't support UTF-8.

  There functions that allows conversion from our strings to the C-style string for printing!

  @Important: Strings are immutable, to change value of the string, either:
  - Copy its data into allocated memory on the stack or heap.
  - Use copy_string() to copy the string to the heap.
  - Sometimes use sprint() will solve your needs.
 */

#pragma once

#include "common.h"

// This is for the comparisons
struct String;
bool equal(String a, String b);

bool equal(char a, char b);

// Again, Strings are immutable (saying it twice because I will definitely forget)
struct String
{
    i64 count = 0;    // Amount of characters in the string
    u8 *data  = NULL; // Pointer to the start of the characters

    //
    // Helper functions below this line
    //

    explicit String()
    {
    }

    // This function only creates strings that exists on the stack
    explicit String(const char c_string[]) // creates String from a string constant
    {
        if (c_string == NULL) return;

        count = strlen(c_string);

        // Because we are making strings on the stack here,
        // we straight up assign the data pointer to the pointer
        // of the given c_string
        data = (u8*)c_string; // Safe cast
    }
    
    // explicit String(my_string std_string)
    // {
    //     // Figure this out later
    //     // Because std::string are allocatd on the stack but its memory lives
    //     // in the heap, doing this is quite difficult.
    //     // The best solution is to not use std::string at all
    //     assert(0);
    // }

    u8 *begin() { assert((data != NULL) & (count > 0)); return &data[0]; }
    u8 *end()   { assert((data != NULL) & (count > 0)); return &data[count]; }

    operator bool() { return count > 0; };
    String &operator=(const char c_string[]) // assigns string constant to String
    {
        if (c_string == NULL) count = 0; 
        else                  count = strlen(c_string);
        data = (u8*)c_string;
        return *this;
    }
    u8 operator[](i64 index)
    {
        assert(index < count);
        assert(index >= 0);
        assert(data != NULL);

        return data[index];
    }
    bool operator==(String rhs) {return equal(*this, rhs);}
    bool operator!=(String rhs) {return !equal(*this, rhs);}
};

//
// Comparisons
bool equal(String a, String b);                   // case sensitive
bool equal_nocase(String a, String b);            // case insensitive
i32  compare(String a, String b);                 // case sensitive
i32  compare_nocase(String a, String b);          // case insensitive
bool contains(String str, String substring);
bool contains(String str, u8 byte);
bool begins_with(String str, String prefix);
bool ends_with(String str, String suffix);
inline __attribute__((always_inline))
bool ends_with(String str, u8 suffix)
{
    if (!str) return false;
    return str.data[str.count - 1] == suffix;
}

inline __attribute__((always_inline))
void free_string(String *s)
{
    my_free((void*)s->data);
    s->count = 0;
}

//
// Find
i64 find_index_from_left(String str, u8 byte);
// i64 find_index_from_left(String str, String substring);
i64 find_index_from_right(String str, u8 byte);
// i64 find_index_from_right(String str, String substring);


// 
// Allocation
// @Important: need to free the returned c_string
u8 *to_c_string(String s, Allocator allocator = {});

// Uses temporary storage
u8 *temp_c_string(String s);

Allocator get_allocator(Allocator  parameter_allocator = {},
                        _type_Type type = _make_Type(void));
String alloc_string(i64       nbytes_excluding_zero,
                    Allocator allocator);


void *__temporary_allocator(Allocator_Mode mode,
                            i64 size, i64 old_size,
                            void *old_memory, void *allocator_data);

// Uses temporary storage
inline
String talloc_string(i64 nbytes_excluding_zero)
{
    Allocator a = {global_context.temporary_storage, __temporary_allocator};
    return alloc_string(nbytes_excluding_zero, a);
}

inline __attribute__((always_inline))
String empty(bool null_terminate)
{
    if (null_terminate)
    {
        auto a = get_allocator();
        auto empty = alloc_string(1, a);
        empty.data[0] = '\0';
        return empty;
    }
    else
    {
        return String("");
    }
}

// Copy string
// @Important: REMEMBER TO ASSIGN AND FREE WHEN USED
String copy_string(String str, Allocator allocator = {}, bool null_terminate = false);

//
// Join
// Forward declare some stuff here because of circular dependency
template <typename V>
struct Resizable_Array;
template <typename V>
using RArr = Resizable_Array<V>;

struct Allocator;

// @Important: REMEMBER TO ASSIGN AND FREE WHEN USED
__attribute__((warn_unused_result))
String join(RArr<String> inputs,
            Allocator    allocator = {},
            bool         null_terminate = false);
// @Important: REMEMBER TO ASSIGN AND FREE WHEN USED
__attribute__((warn_unused_result))
String join(RArr<String> inputs,         String separator,
            Allocator    allocator = {}, bool   null_terminate = false);

//
// Split
RArr<String> split(String text, String separator, bool reversed = false, i64 max_results = 0,
                   bool skip_empty = false, bool keep_separator = false, Allocator = {}) __attribute__((warn_unused_result));

//
// sprint
// @Important: REMEMBER TO ASSIGN AND FREE WHEN USED
template <typename... Args>
__attribute__((warn_unused_result))
String sprint(String fmt, Args... args) // Hastily done because snprintf is very complicated :(
{
    // @Fixme: For now, it uses the context allocator
    Allocator a = get_allocator();
    String result;

    const i64 RESULTING_LENGTH = snprintf(NULL, 0, (char*)(fmt.data), args...);

    result.data  = (unsigned char*)(a.proc(Allocator_Mode::ALLOCATE, RESULTING_LENGTH + 1, 0, NULL, a.data));
    result.count = RESULTING_LENGTH;

    assert((snprintf((char*)(result.data), RESULTING_LENGTH + 1,
                     (char*)(fmt.data), args...) == result.count));

    return result;
}

//
// tprint: uses the temporary storage allocator
template <typename... Args>
__attribute__((warn_unused_result))
String tprint(String fmt, Args... args)
{
    Allocator a = {global_context.temporary_storage, __temporary_allocator};
    String result;

    const i64 RESULTING_LENGTH = snprintf(NULL, 0, (char*)(fmt.data), args...);

    result.data  = (unsigned char*)(a.proc(Allocator_Mode::ALLOCATE, RESULTING_LENGTH + 1, 0, NULL, a.data));
    result.count = RESULTING_LENGTH;

    assert((snprintf((char*)(result.data), RESULTING_LENGTH + 1,
                     (char*)(fmt.data), args...) == result.count));

    return result;
}

__attribute__((warn_unused_result))
my_pair<String, bool> copy_substring(String s, i64 index, i64 bytes, Allocator allocator = {});
