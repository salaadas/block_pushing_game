// @Cleanup: Refactor the 'Error at line %d' thing into a error handling procedure inside file_utils.

#include "vars.h"

#include "file_utils.h"
#include "sokoban.h"

#include "visit_struct.h"
namespace vs = visit_struct;

struct Value_Slot
{
    String     name; // This is not allocated.
    i64        byte_offset;
    _type_Type type = _make_Type(void);
};

struct Value_Folder
{
    String name; // This is not allocated. It also contains some garbage value at the start, so we need to use contains() instead of equal() to compare against the *.variable files.
    u8    *data;

    RArr<Value_Slot> members;
};

RArr<Value_Folder> folders;

template <typename T>
void add_struct_to_folders(T *folder_pointer)
{
    _type_Type folder_type = _make_Type(T);

    if (!std::is_class<T>::value)
    {
        logprint("add_struct_to_folders", "Error! Folder with name '%s' is not a type of struct...\n", folder_type.name());
        return;
    }

    Value_Folder folder;
    folder.name = folder_type.name();
    folder.data = reinterpret_cast<u8*>(folder_pointer);

    // Fill out the members attributes.
    vs::for_each(*folder_pointer, [&](const char *member_name, auto &member_ref_value) {
        u8 *member_pointer     = reinterpret_cast<u8*>(&member_ref_value);
        i64 member_byte_offset = member_pointer - folder.data;

        Value_Slot slot;
        slot.name        = String(member_name);
        slot.byte_offset = member_byte_offset;

        auto dummy = member_ref_value; // To strip away the reference attribute.
        slot.type = _make_Type(member_ref_value);

        array_add(&folder.members, slot);
    });

    array_add(&folders, folder);
}

// @Cleanup:
void poke_value(Value_Folder *folder, Value_Slot *slot, String rhs, char *c_agent, u32 line_number)
{
// Helper functions
#define poke_float(float_type)                                          \
    auto data = reinterpret_cast<float_type*>(folder->data + slot->byte_offset); \
    bool success = false;                                               \
    auto [value, remainder] = string_to_float(rhs, &success);           \
    if (!success) {logprint(c_agent, "Error at line %d! Was not able to parse "#float_type" value '%s'.\n", line_number, temp_c_string(rhs)); return;} \
    if (remainder) logprint(c_agent, "Warning at line %d! Junk at the end of line '%s'.\n", line_number, temp_c_string(remainder)); \
    *data = value;                                                      \

#define poke_int(int_type)                                              \
    auto data = reinterpret_cast<int_type*>(folder->data + slot->byte_offset); \
    bool success = false;                                               \
    auto [value, remainder] = string_to_int(rhs, &success);             \
    if (!success) {logprint(c_agent, "Error at line %d! Was not able to parse "#int_type" value '%s'.\n", line_number, temp_c_string(rhs)); return;} \
    if (remainder) logprint(c_agent, "Warning at line %d! Junk at the end of line '%s'.\n", line_number, temp_c_string(remainder)); \
    *data = value;                                                      \

    if (cmp_var_type_to_type(slot->type, i32)) {poke_int(i32);}
    else if (cmp_var_type_to_type(slot->type, i64)) {poke_int(i64);}
    else if (cmp_var_type_to_type(slot->type, u32)) {poke_int(u32);}
    else if (cmp_var_type_to_type(slot->type, u64)) {poke_int(u64);}
    else if (cmp_var_type_to_type(slot->type, f32)) {poke_float(f32);}
    else if (cmp_var_type_to_type(slot->type, f64)) {poke_float(f64);}
    else if (cmp_var_type_to_type(slot->type, bool))
    {
        auto data = reinterpret_cast<bool*>(folder->data + slot->byte_offset);
        bool value = false;

        if (rhs == String("true"))       value = true;
        else if (rhs == String("false")) value = false;
        else
        {
            logprint(c_agent, "Error at line %d! Was not able to parse boolean value '%s'.\n", line_number, temp_c_string(rhs));
            return;
        }

        *data = value;
    }
    else if (cmp_var_type_to_type(slot->type, String))
    {
        auto enclosed_in_quotes = (((rhs[0] == '\'') && (rhs[rhs.count - 1] == '\'')) ||
                                   ((rhs[0] == '"') && (rhs[rhs.count - 1] == '"')));
        if (!enclosed_in_quotes)
        {
            logprint(c_agent, "Warning at line %d! String value must be delimited inside single or double quotes, found '%s'!\n", line_number, temp_c_string(rhs));
            return;
        }

        advance(&rhs, 1);
        rhs.count -= 1;

        auto data = reinterpret_cast<String*>(folder->data + slot->byte_offset);

        // @Note: Our policy with string variables here is that we leak pre-existing strings when we hotload 
        // a file. Realistically, this only happens during development when you hotload a change in the file. 
        // Also, because this magnitude of leak is so small that we should not care about.

        String s = copy_string(rhs);
        *data = s;
    }
    else if (cmp_var_type_to_type(slot->type, Vector4))
    {
        auto enclosed_in_parens = (rhs[0] == '(') && (rhs[rhs.count - 1] == ')');

        if (!enclosed_in_parens)
        {
            logprint(c_agent, "Warning at line %d! Vector4 value must be delimited inside parenthesis, e.g. (x y z w), found '%s'!\n", line_number, temp_c_string(rhs));
            return;
        }

        advance(&rhs, 1);
        rhs.count -= 1;

        auto data = reinterpret_cast<Vector4*>(folder->data + slot->byte_offset);

        bool success = false;
        auto [value, remainder] = string_to_vec4(rhs, &success);

        if (!success)
        {
            logprint(c_agent, "Error at line %d! Not able to parse Vector4 value '%s'.\n", line_number, temp_c_string(rhs));
            return;
        }

        if (remainder) logprint(c_agent, "Warning at line %d! Junk at the end of line '%s'.\n", line_number, temp_c_string(remainder));

        *data = value;
    }
    else
    {
        logprint(c_agent, "Error at line %d! poking of type %s in value '%s' is not supported!\n", line_number, slot->type.name(), temp_c_string(rhs));
        assert(0);
    }

#undef poke_float
#undef poke_int
}

void reload_variables_really(String full_name, bool optional = false); // @ForwardDeclare

void reload_variables(String short_name, String full_name)
{
    if (short_name == String("All"))
    {
        // Any time we reload All, also reload Local afterward,
        // since Local layers on top of All.

        reload_variables_really(full_name);
        reload_variables_really(String("data/Local.variables"), true); // @Hardcode:
    }
    else if (short_name == String("Local"))
    {
        // Any time we reload Local, reload All first,
        // so that we correctly set variables back to their defaults.

        reload_variables_really(String("data/All.variables"), false); // @Hardcode:
        reload_variables_really(full_name);
    }
    else
    {
        reload_variables_really(full_name);
    }
}

void reload_variables_really(String full_name, bool optional)
{
    Text_File_Handler handler;
    String agent("variables");

    start_file(&handler, full_name, agent, optional);
    if (handler.failed) return;

    auto c_agent = reinterpret_cast<char*>(temp_c_string(agent));
    defer { deinit(&handler); };

    Value_Folder *current_folder = NULL;

    while (true)
    {
        auto [line, found] = consume_next_line(&handler);
        if (!found) break;
        assert(line);

        if (line[0] == ':')
        {
            if (line.count < 2)
            {
                logprint(c_agent, "Error at line %d! Line starting with ':' must have a '/' and a name after.\n", handler.line_number);
            }
            else
            {
                if (line[1] != '/')
                {
                    logprint(c_agent, "Error at line %d! Expected a '/' after ':'.\n", handler.line_number);
                }
                else
                {
                    advance(&line, 2);
                    auto folder_name = line;

                    bool found = false;
                    for (auto &it : folders)
                    {
                        if (contains(it.name, folder_name))
                        {
                            current_folder = &it;
                            found = true;
                            break;
                        }
                    }

                    if (!found)
                    {
                        logprint(c_agent, "Error at line %d! Not able to find a value folder with name '%s'.\n", handler.line_number, temp_c_string(folder_name));
                        continue;
                    }
                }
            }
        }
        else
        {
            auto rhs = line;
            while (true)
            {
                if (!rhs) break;
                if (isspace(rhs[0])) break;

                advance(&rhs, 1);
            }

            if (!rhs)
            {
                logprint(c_agent, "Error at line %d! Expected a space after variable name.\n", handler.line_number);
                continue;
            }

            auto name = line;
            name.count -= rhs.count;

            eat_spaces(&rhs);

            if (!current_folder)
            {
                logprint(c_agent, "Error at line %d! Did not specify which folder to poke value '%s'.", handler.line_number, temp_c_string(name));
                continue;
            }

            Value_Slot *found = NULL;
            for (auto &it : current_folder->members)
            {
                if (it.name == name)
                {
                    found = &it;
                }
            }

            if (!found)
            {
                logprint(c_agent, "Error at line %d! Was not able to find member variable with name '%s' inside struct folder '%s'!\n", handler.line_number, temp_c_string(name), current_folder->name.data);
                continue;
            }

            poke_value(current_folder, found, rhs, c_agent, handler.line_number);
        }
    }
}

void init_variables()
{
    add_struct_to_folders(&gameplay_visuals);

    reload_variables(String("All"), String("data/All.variables"));
}
