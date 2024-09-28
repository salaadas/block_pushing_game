// https://github.com/onelivesleft/jai-string/blob/main/Strings_Alloc/Strings_Alloc.jai

#include "newstring.h"
#include <cctype>

// @Important: Must have defined 'allocator' and 'result' beforehand!
#define set_result_allocator()                                               \
    if                     (allocator) result = allocator;                   \
    else if (global_context.allocator) result = global_context.allocator;    \
    else                               result = {NULL, __default_allocator};  

// @Note: Call get_allocator first before calling this
String alloc_string(i64 nbytes_excluding_zero,
                    Allocator allocator)
{
    auto count = nbytes_excluding_zero;
    assert(count >= 0);

    String s;
    s.count = count;
    s.data  = static_cast<u8*>(allocator.proc(Allocator_Mode::ALLOCATE, count, 0, NULL, allocator.data));

    return s;
}

Allocator get_allocator(Allocator parameter_allocator,
                        _type_Type type)
{
    if (parameter_allocator)
        return parameter_allocator;
    else if (global_context.allocator)
        return global_context.allocator;
    else
        return {NULL, __default_allocator};
}

bool equal(String a, String b)
{
    if (a.count != b.count) return false;
    return compare(a, b) == 0;
}

bool equal(char a, char b)
{
    return a == b;
}

bool equal_nocase(String a, String b)
{
    // Mostly the same as equal(...)
    if (a.count != b.count) return false;
    return compare_nocase(a, b) == 0;
}

// -1 if a <  b
//  0 if a == b
//  1 if a >  b
i32 compare(String a, String b)
{
    if (a.count > b.count) return -1 * compare(b, a);

    i64 it = 0;
    while ((it < a.count) && (a[it] == b[it])) ++it;

    if (it == a.count)
    {
        // Example: compare("fun", "fun")
        if (it == b.count) return 0;
        else
        {
            // Example: compare("dog", "dog2")
            // Treat the shorter one as of it has trailing 0's
            return 0 - b[it];
        }
    }

    // Example: compare("fat", "far")
    return a[it] - b[it];
}

i32 compare_nocase(String a, String b)
{
    if (a.count > b.count) return -1 * compare(b, a);

    i64 it = 0;
    // -----------------------v This is the only difference between compare(...) and compare_nocase(...)
    while ((it < a.count) && (tolower(a[it]) == tolower(b[it]))) ++it;

    if (it == a.count)
    {
        if (it == b.count) return 0;
        else               return 0 - b[it];
    }

    return a[it] - b[it];
}

bool contains(String str, String substring)
{
    if (substring.count > str.count) return false;
    
    i64 it = 0;
    while (it <= (str.count - substring.count))
    {
        for (i64 sub_it = 0;
             (sub_it < substring.count) && (str[it + sub_it] == substring[sub_it]);
             ++sub_it)
        {
            if (sub_it == (substring.count - 1)) return true;
        }

        it += 1;
    }

    return false;
}

// @Note: Ignore the empty byte case
bool contains(String str, u8 byte)
{
    if (str.count == 0) return false;

    for (i64 it = 0; it < str.count; ++it)
        if (str[it] == byte) return true;

    return false;
}

bool begins_with(String str, String prefix)
{
    if (str.count <= 0) return false;

    i64 it = 0;
    while ((it < prefix.count) &&
           (it < str.count) &&
           (str[it] == prefix[it]))
    {
        it += 1;
    }

    return it == prefix.count;
}

bool ends_with(String str, String suffix)
{
    if (str.count <= 0) return false;

    i64 it    = str.count - 1;
    i64 index = suffix.count - 1;
    while ((index >= 0) &&
           (it >= 0) &&
           (str[it] == suffix[index]))
    {
        it    -= 1;
        index -= 1;
    }

    return index == -1;
}

// @Important: need to free the returned c_string
u8 *to_c_string(String s, Allocator allocator)
{
    auto copied_s = copy_string(s, allocator, true);
    return copied_s.data;
}

u8 *temp_c_string(String s)
{
    auto temp_copied_s = copy_string(s, {global_context.temporary_storage, __temporary_allocator}, true);
    return temp_copied_s.data;
}

String copy_string(String string, Allocator allocator, bool null_terminate)
{
    if (!string) return empty(null_terminate);

    String result;
    auto a = get_allocator(allocator);

    if (null_terminate && (string[string.count - 1] != '\0'))
    {
        result = alloc_string(string.count + 1, a);
        memcpy(result.data, string.data, string.count);
        result.data[result.count - 1] = '\0';
    }
    else
    {
        result = alloc_string(string.count, a);
        memcpy(result.data, string.data, string.count);
    }

    return result;
}

#include <cstdarg>
String join(i64 nstrings, ...) // Temporary storage
{
    // First pass to calculate the total length.
    va_list args;
    va_start(args, nstrings);
    i64 len = 0;
    for (i64 it_index = 0; it_index < nstrings; ++it_index)
    {
        auto s = va_arg(args, String);
        len += s.count;
    }
    va_end(args);

    // Second pass to memcpy to the result.
    String result = talloc_string(len);
    i64 cursor = 0;

    va_list args2;
    va_start(args2, nstrings);
    for (i64 it_index = 0; it_index < nstrings; ++it_index)
    {
        auto s = va_arg(args2, String);
        u8 *dest = result.data + cursor;
        u8 *src  = s.data;
        memcpy(dest, src, s.count);

        cursor += s.count;
    }
    va_end(args2);

    return result;
}

String join(RArr<String> inputs,
            Allocator allocator, bool null_terminate)
{
    if (inputs.count == 0) return empty(null_terminate);

    if (inputs.count == 1)
    {
        Allocator result = {};
        set_result_allocator();

        return copy_string(inputs[0], result, null_terminate);
    }

    auto last_string = inputs[inputs.count - 1];
    bool terminating = null_terminate &&
        (last_string.data[last_string.count] != '\0');
    auto extra_byte  = (terminating) ? 1 : 0;

    auto count = 0;
    for (auto it : inputs) count += it.count;

    Allocator result = {};
    set_result_allocator();

    auto joined = alloc_string(count + extra_byte, result);
    memcpy(joined.data, inputs[0].data, inputs[0].count);
    auto position = joined.data + inputs[0].count;

    for (i64 i = 1; i < inputs.count; ++i)
    {
        memcpy(position, inputs[i].data, inputs[i].count);
        position += inputs[i].count;
    }

    if (terminating) joined[position] = '\0';

    return joined;
}

String join(RArr<String> inputs, String separator,
            Allocator allocator, bool null_terminate)
{
    if (inputs.count == 0) return empty(null_terminate);

    if (inputs.count == 1)
    {
        Allocator result = {};
        set_result_allocator();

        return copy_string(inputs[0], result, null_terminate);
    }

    auto last_string = inputs[inputs.count - 1];
    bool terminating = null_terminate &&
        (last_string.data[last_string.count] != '\0');
    auto extra_byte = (terminating) ? 1 : 0;

    auto count = separator.count * (inputs.count - 1);
    for (auto it : inputs) count += it.count;

    Allocator result = {};
    set_result_allocator();

    auto joined = alloc_string(count + extra_byte, result);
    memcpy(joined.data, inputs[0].data, inputs[0].count);
    auto position = joined.data + inputs[0].count;

    for (i64 i = 1; i < inputs.count; ++i)
    {
        if (separator)
        {
            memcpy(position, separator.data, separator.count);
            position += separator.count;
        }
        memcpy(position, inputs[i].data, inputs[i].count);
        position += inputs[i].count;
    }

    if (terminating) joined.data[count] = '\0';
    return joined;
}

i64 find_index_from_left(String str, u8 byte)
{
    for (i64 it = 0; it < str.count; ++it)
        if (str[it] == byte) return it;
    return -1;
}

i64 find_index_from_left(String str, String substring)
{
    assert(0);
}

i64 find_index_from_right(String str, u8 byte)
{
    for (i64 it = str.count - 1; it >= 0; --it)
        if (str[it] == byte) return it;
    return -1;
}

i64 find_index_from_right(String str, String substring)
{
    assert(0);
}

my_pair<String, bool> copy_substring(String s, i64 index, i64 bytes, Allocator allocator)
{
    String result;
    if (s.count < (index + bytes))
    {
        return {String(""), false};
    }

    auto a = get_allocator(allocator);
    result = alloc_string(bytes, a);
    if (!result.data) return {String(""), false};

    memcpy(result.data, s.data + index, bytes);

    return {result, true};
}

// // Split
// RArr<String> split(String text, String separator, bool reversed, i64 max_results,
//                    bool skip_empty, bool keep_separator, Allocator allocator)
// {
//     assert(0);
// }


/*
#include "array.h"
SArr<String> split(String string, String separator)
{
}

//
// Modifying

String trim_left(String str)
{
}

String trim_right(String str)
{
}

String trim(String str)
{
}

void to_lower_in_place(String str)
{
}

void to_upper_in_place(String str)
{
}

*/
