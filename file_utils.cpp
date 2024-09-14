#include "file_utils.h"

#include <glob.h>

// Visits everything in one folder (non-recursively). In our program, this folder is
// the same as the folder of the executable.
// If it finds a directory  that doesn't start with '.', call the visit proc on that directory.
bool visit_files(String dir_name, void *data, FileVisitProc visit_proc, bool only_directory)
{
    glob_t globlist;

    auto wildcard_name = tprint(String("%s/*"), temp_c_string(dir_name));

    auto flags = GLOB_PERIOD;

    if (only_directory)
    {
        flags |= GLOB_ONLYDIR;
    }

    auto glob_error = glob((char*)temp_c_string(wildcard_name), flags, NULL, &globlist);
    if (glob_error != 0)
    {
        return false;
    }

    for (i32 i = 0; globlist.gl_pathv[i]; ++i)
    {
        // Both files and dirs
        String full_name(globlist.gl_pathv[i]);

        auto last_slash = find_index_from_right(full_name, '/');

        if (full_name[last_slash + 1] != '.')
        {
            String short_name = talloc_string(full_name.count - last_slash - 1);

            i64 fi = last_slash + 1;
            for (i64 si = 0; si < short_name.count; ++si, ++fi)
                short_name.data[si] = full_name[fi];

            visit_proc(short_name, full_name, data);
        }
    }

    globfree(&globlist);;
    return true;
}

// @Important: Be sure to free the memory of the string once used
my_pair<String, bool> read_entire_file(String full_path)
{
    auto c_path = temp_c_string(full_path);
    FILE *file_stream = fopen((char*)c_path, "rb");
    if (!file_stream) return {String(""), false};

    i32 success = fseek(file_stream, 0, SEEK_END);
    if (success == -1) return {String(""), false};

    i64 buffer_size = ftell(file_stream);

    success = fseek(file_stream, 0, SEEK_SET);
    if (success == -1) return {String(""), false};

    // Using the default allocator
    String result = alloc_string(buffer_size, global_context.allocator);
    fread(result.data, buffer_size, 1, file_stream);

    fclose(file_stream);

    return {result, true};
}

void advance(String *s, i64 amount)
{
    assert(amount >= 0);
    assert(s->count >= amount);

    s->data  += amount;
    s->count -= amount;
}

String find_character_from_right(String string, u8 c)
{
    auto cursor = string.count - 1;

    while (cursor >= 0)
    {
        if (string[cursor] == c)
        {
            String substring = string;

            substring.data += cursor;
            substring.count = string.count - cursor;

            return substring;
        }

        cursor -= 1;
    }

    return String("");
}

String find_character_from_left(String string, u8 c)
{
    auto cursor = 0;
    while (cursor < string.count)
    {
        if (string[cursor] == c)
        {
            String substring = string;

            substring.count = cursor + 1;

            return substring;
        }

        cursor += 1;
    }

    return String("");
}

void eat_trailing_spaces(String *string)
{
    auto cursor = string->count - 1;
    while (cursor >= 0)
    {
        auto c = string->data[cursor];

        if (c == ' ' || c == '\t' || c == '\n') cursor -= 1;
        else break;
    }

    string->count = cursor + 1;
}

my_pair<String, bool> consume_next_line(Text_File_Handler *handler)
{
    while (true)
    {
        auto [line, found] = consume_next_line(&handler->file_data);
        if (!found) return {String(""), false};

        handler->line_number += 1;

        eat_spaces(&line);

        if (!line) continue;

        if (handler->strip_comments_from_ends_of_lines)
        {
            auto lhs = find_character_from_left(line, handler->comment_character);

            if (lhs)
            {
                line = lhs;
                line.count -= 1;
                if (line.count <= 0) continue;
            }
        }

        if (line[0] == handler->comment_character) continue;

        eat_trailing_spaces(&line);
        assert(line.count > 0);

        return {line, found};
    }
}

my_pair<String, bool> consume_next_line(String *sp)
{
    auto s = *sp;
    auto t = find_index_from_left(s, '\n');

    if (t == -1)
    {
        if (s.count)
        {
            t = sp->count;
        }
        else
        {
            return {s, false};
        }
    }

    auto result = talloc_string(t);

    for (i32 i = 0; i < t; ++i)
    {
        result.data[i] = s.data[i];
    }

    auto removal_count = t + 1;
    if (removal_count > sp->count) removal_count = sp->count;

    advance(sp, removal_count);

    return {result, true};
}

void eat_spaces(String *sp)
{
    while (true)
    {
        if (!sp->count) return;

        auto c = sp->data[0];

        if (!isspace(c)) return;

        sp->data  += 1;
        sp->count -= 1;
    }
}

void deinit(Text_File_Handler *handler)
{
    free_string(&handler->orig_file_data);
}

// @Todo: cleanup this and string_to_float
my_pair<i64, String> string_to_int(String s, bool *success)
{
    eat_spaces(&s);

    if (!s)
    {
        *success = false;
        return {0, s};
    }

    i64 i = 0;

    // if (!s || !isdigit(s[i]))
    // {
    // }

    String int_up_to_space = s;

    for (auto c : int_up_to_space)
    {
        if (isdigit(c))                {/* We are good! */}
        else if (c == '-')             {/* We are good! */}
        else if (c == 'e')             {/* We are good! */}
        else break;

        i += 1;
    }

    int_up_to_space.count = i;
    auto c_int = temp_c_string(int_up_to_space);

    i64 result = 0;
    auto total_read = sscanf((char*)c_int, "%ld", &result); // @Fixme: Convert from sscanf to atoi

    if (total_read != 1)
    {
        *success = false;
        return {0, s};
    }

    advance(&s, i);

    *success = true;
    return {result, s};
}

// @Todo: revisit this later, currently not good
my_pair<f32, String> string_to_float(String s, bool *success)
{
    eat_spaces(&s);

    if (!s)
    {
        *success = false;
        return {0.0, s};
    }

    i64 i = 0;

    // if (!s || (!isdigit(s[i]) && (s[i] != '.')))
    // {
    //     *success = false;
    //     return {0.0, s};
    // }

    String float_up_to_space = s;

    bool has_encounter_f = false;

    for (auto c : float_up_to_space)
    {
        if (isdigit(c))                {/* We are good! */}
        else if (c == '-' || c == '.') {/* We are good! */}
        else if (c == 'e')             {/* We are good! */}
        else if (isalpha(c))
        {
            if (c == 'f')
            {
                i += 1;
            }

            break;
        }
        else break;

        i += 1;
    }

    float_up_to_space.count = i;
    auto c_float = (char*)temp_c_string(float_up_to_space);

    f32 result = 0;

    auto scan_read = sscanf((char*)c_float, "%f", &result);  // @Fixme: Convert from sscanf to atof
    if (scan_read != 1)
    {
        *success = false;
        return {0.0, s};
    }

    advance(&s, i);

    *success = true;
    return {result, s};
}

my_pair<Vector4, String> string_to_vec4(String s, bool *success)
{
    Vector4 result;

    String orig_string;

    f32    value;
    String remainder;
    bool   f_success;

    my_pair<f32, String> p;

    p = string_to_float(s, &f_success);
    value     = p.first;
    remainder = p.second;
    *success = f_success;
    if (!f_success) return {Vector4(0, 0, 0, 0), orig_string};
    result.x = value;

    p = string_to_float(remainder, &f_success);
    value     = p.first;
    remainder = p.second;
    *success = f_success;
    if (!f_success) return {Vector4(0, 0, 0, 0), orig_string};
    result.y = value;

    p = string_to_float(remainder, &f_success);
    value     = p.first;
    remainder = p.second;
    *success = f_success;
    if (!f_success) return {Vector4(0, 0, 0, 0), orig_string};
    result.z = value;

    p = string_to_float(remainder, &f_success);
    value     = p.first;
    remainder = p.second;
    *success = f_success;
    if (!f_success) return {Vector4(0, 0, 0, 0), orig_string};
    result.w = value;

    return {result, remainder};
}

my_pair<String, String> break_by_spaces(String line)
{
    eat_spaces(&line);

    i64 i = 0;
    String first_half = line;

    for (auto c : first_half)
    {
        if (isspace(c)) break;
        i += 1;
    }

    first_half.count = i;
    advance(&line, i);

    eat_spaces(&line);

    return {first_half, line};
}

void start_file(Text_File_Handler *handler, String full_path, String log_agent)
{
    // @Important: We do NOT copy any of these string; we presume they remain valid
    // throughout the lifetime of the handler.

    handler->full_path = full_path;
    handler->log_agent = log_agent;

    auto [file_data, success] = read_entire_file(full_path);
    handler->file_data      = file_data;
    handler->orig_file_data = file_data;

    char *c_log_agent = (char*)temp_c_string(log_agent);
    char *c_full_path = (char*)temp_c_string(full_path);

    if (!success)
    {
        logprint(c_log_agent, "Unable to load file '%s'.\n", c_full_path);
        handler->failed = true;
        return;
    }

    if (handler->do_version_number)
    {
        // Parse the verison number.
        auto [line, found] = consume_next_line(&handler->file_data);
        handler->line_number += 1;

        if (!found)
        {
            logprint(c_log_agent, "Unable to find a version number at the top of file '%s'!", c_full_path);
            handler->failed = true;
            return;
        }

        assert(line.count > 0);

        if (line[0] != '[')
        {
            logprint(c_log_agent, "Expected '[' at the top of file '%s', but did not get it!\n", c_full_path);
            handler->failed = true;
            return;
        }

        advance(&line, 1);

        bool version_success = false;
        auto [version, remainder] = string_to_int(line, &version_success);
        
        if (!version_success)
        {
            logprint(c_log_agent, "Unable to parse the version number at the top of file '%s'!\n", c_full_path);
            handler->failed = true;
            return;
        }

        if ((!remainder.count) || (remainder[0] != ']'))
        {
            logprint(c_log_agent, "Expected ']' after version number in file '%s', but did not get it! (Remainder is: '%s')\n", c_full_path, temp_c_string(remainder));
            handler->failed = true;
            return;
        }

        handler->version = version;
    }
}
