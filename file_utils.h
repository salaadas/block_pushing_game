#pragma once

#include "common.h"
#include <cctype>

template <typename U, typename V>
void Swap(U *a, V *b)
{
    auto temp = *a;
    *a = *b;
    *b = temp;
}

// #define MYLOGCONCAT(a, b) a b

// #define logprint(ident, ...)                                                              \
//     printf(MYLOGCONCAT("[", ident) "] Function '%s', line %d: ", __FUNCTION__, __LINE__); \
//     printf(__VA_ARGS__);

struct Text_File_Handler
{
    String full_path;

    String log_agent;

    char comment_character = '#';

    bool do_version_number = true;
    // If you switch this to false, things might be faster, but not by much...
    bool strip_comments_from_ends_of_lines = true;

    String file_data;
    String orig_file_data;

    bool failed = false;
    i32  version = -1; // Set when parsing the file if do_version_number = true

    u32 line_number = 0;
};

typedef void(FileVisitProc)(String short_name, String full_name, void *data);
bool visit_files(String matching_pattern, void *data, FileVisitProc visit_proc, bool only_directory = false);

my_pair<String, bool> read_entire_file(String full_path);
my_pair<String, bool> consume_next_line(String *sp);

String find_character_from_right(String string, u8 c);
String find_character_from_left(String string, u8 c);
void advance(String *s, i64 amount);
void eat_spaces(String *sp);
my_pair<String, String> break_by_spaces(String line);

void start_file(Text_File_Handler *handler, String full_path, String log_agent);
void deinit(Text_File_Handler *handler);
my_pair<String, bool> consume_next_line(Text_File_Handler *handler);

my_pair<f32, String> string_to_float(String s, bool *success);
my_pair<i64, String> string_to_int(String s, bool *success);
my_pair<Vector4, String> string_to_vec4(String s, bool *success);
