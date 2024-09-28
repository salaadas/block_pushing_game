//
// For the shader include system, here is how it currently works:
// If you want to include a shader file, for example 'pbr.gl', what
// you would do is that you would is that you would say:
//                    // @@include "pbr.gl"
// In your file and it should copy paste the content of the file inside
// your buffer. This should work for hotreloading the shaders too!
//
// There is a definition of header '.glh' file where it does not compile into
// a shader thing itself, but rather just a thing for people to include in.
//

#include "shader_catalog.h"

#include "opengl.h"
#include "file_utils.h"

void init_shader(Shader *shader)
{
    // @Note: This is hard-coded to the XCNUU format rightnow (handle XCNUUs later)
    // @Note: This has tremendous errors because some files does not contains normals, so the ordering is busted
    shader->position_loc      = 0;
    shader->color_scale_loc   = 1;
    shader->normal_loc        = 2;
    shader->uv_0_loc          = 3;
    shader->uv_1_loc          = 4;

    shader->lightmap_uv_loc   = 4;
    shader->blend_weights_loc = 5;
    shader->blend_indices_loc = 6;

    shader->diffuse_texture_wraps = true;
    shader->textures_point_sample = true;

    shader->alpha_blend   = true;
    shader->depth_test    = true;
    shader->depth_write   = true;
    shader->backface_cull = true;

    shader->loaded = false;
}

void init_shader_catalog(Shader_Catalog *catalog)
{
    catalog->base.my_name = String("shaders");
    array_add(&catalog->base.extensions, String("gl"));
    array_add(&catalog->base.extensions, String("glh"));

    do_polymorphic_catalog_init(catalog);
}

void deinit_shader_catalog(Shader_Catalog *catalog)
{
    array_free(&catalog->base.extensions);

    for (auto it : catalog->table)
    {
        free_string(&it.key);

        auto shader = it.value;
        free_string(&shader->name);
        free_string(&shader->full_path);
        my_free(shader);
    }

    deinit(&catalog->table);
}

Shader *make_placeholder(Shader_Catalog *catalog, String short_name, String full_path)
{
    Shader *shader    = New<Shader>();
    init_shader(shader);
    shader->name      = copy_string(short_name);
    shader->full_path = copy_string(full_path);

    if (get_extension(shader->full_path) == String("glh"))
    {
        shader->is_header = true;
    }

    return shader;
}

bool load_shader_from_memory(String shader_text, Shader *shader)
{
    auto short_name = shader->name;

    if (!shader_text.count)
    {
        logprint("shader_catalog", "Shader '%s' contains nothing in content, so we couldn't load it!\n", temp_c_string(short_name));

        return false;
    }

    shader->vertex_shader   = glCreateShader(GL_VERTEX_SHADER);
    shader->fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);

    // @Note: Shader must be zero-terminated
    char *c_s_text = (char*)to_c_string(shader_text);
    defer { my_free(c_s_text); };

    String vert_string = sprint(String("#version 430 core\n#define VERTEX_SHADER\n#define COMM out\n%s"), c_s_text);
    String frag_string = sprint(String("#version 430 core\n#define FRAGMENT_SHADER\n#define COMM in\n%s"), c_s_text);

    GLint vert_length = vert_string.count;
    GLint frag_length = frag_string.count;

    char *c_v = (char*)to_c_string(vert_string);
    char *c_f = (char*)to_c_string(frag_string);

    glShaderSource(shader->vertex_shader, 1, &c_v, &vert_length);
    glCompileShader(shader->vertex_shader);
    DumpShaderInfoLog(shader->vertex_shader, short_name);

    glShaderSource(shader->fragment_shader, 1, &c_f, &frag_length);
    glCompileShader(shader->fragment_shader);
    DumpShaderInfoLog(shader->fragment_shader, short_name);

    shader->program = glCreateProgram();
    glAttachShader(shader->program, shader->vertex_shader);
    glAttachShader(shader->program, shader->fragment_shader);
    glLinkProgram(shader->program);
    DumpProgramInfoLog(shader->program, short_name);

    glDeleteShader(shader->vertex_shader);
    glDeleteShader(shader->fragment_shader);

    my_free(c_v);
    my_free(c_f);
    free_string(&vert_string);
    free_string(&frag_string);

    // @Note Setting the locations for the uniforms
#define shader_get_uniform(s, loc)                                      \
    {auto c = s; loc = glGetUniformLocation(shader->program, c); DumpGLErrors(#s " uniform");}

    shader_get_uniform("transform",               shader->transform_loc);
    // printf("transform location is: %d\n",         shader->transform_loc);
    shader_get_uniform("blend_matrices",          shader->blend_matrices_loc);
    // printf("blend_matrices location is: %d\n",    shader->blend_matrices_loc);

    shader_get_uniform("diffuse_texture",         shader->diffuse_texture_loc);
    // printf("diffuse location is: %d\n",           shader->diffuse_texture_loc);
    shader_get_uniform("lightmap_texture",        shader->lightmap_texture_loc);
    // printf("lightmap location is: %d\n",          shader->lightmap_texture_loc);
    shader_get_uniform("blend_texture",           shader->blend_texture_loc);
    // printf("blend_texture_loc location is: %d\n", shader->blend_texture_loc);
    shader_get_uniform("normal_map_texture",      shader->normal_map_texture_loc);

#undef shader_get_uniform

    return true;
}

inline
bool is_comment(String s)
{
    return (s.count >= 2) && (s[0] == '/') && (s[1] == '/');
}

// @Todo: Improve the syntax of the include thing by not prefix it with the // because that kinda means
// we should not include it?
// Because of this, if we want to comment out stuff right now, we have to do a double comment thing.
inline
String is_our_custom_directive(String s)
{
    // Example:           '// @@NoBlend'
    // or                 '// @@include "pbr.glh"'
    if (!is_comment(s)) return String("");

    advance(&s, 2); // Advance by the comment size.
    eat_spaces(&s);

    if ((s.count >= 2) && (s[0] == '@') && (s[1] == '@'))
    {
        advance(&s, 2); // Advance by the 2 '@'.
        return s;
    }

    return String("");
}

#include "string_builder.h"

// The success thing indicates whether we should skip the line or not when processing the shader file.
bool parse_shader_include(String relative_include_path, Shader *shader, Shader_Catalog *catalog, String_Builder *builder, i32 line_number)
{
    // Replace the include with the file content.
    auto path_excluding_file_name = shader->full_path;

    // Subtracting one here because we actually want to have the last slash '/'.
    path_excluding_file_name.count -= find_character_from_right(shader->full_path, '/').count - 1;

    if (!relative_include_path)
    {
        logprint("shader_catalog", "In file '%s' line %d, tried to use the include directive but found no input file!\n", temp_c_string(shader->full_path), line_number);
        return false;
    }

    // Remove the quotes from the file name if we leave it on.
    if (relative_include_path[0] == '"') advance(&relative_include_path, 1);
    if (relative_include_path[relative_include_path.count - 1] == '"') relative_include_path.count -= 1;

    // Get the name of the included shader without extension or path leading up to it.
    {
        auto included_last_slash    = find_index_from_right(relative_include_path, '/');
        String included_shader_name = relative_include_path;

        // If we cannot find a slash in the name, we take the whole string;
        // else, we chop off the part leading up to the last slash.
        if (included_last_slash != -1)
        {
            advance(&included_shader_name, included_last_slash + 1);
        }

        auto extension = find_character_from_right(included_shader_name, '.');

        included_shader_name.count -= extension.count; // Strip away the extension.

        if (extension) advance(&extension, 1);

        if (!extension || !array_find(&catalog->base.extensions, extension))
        {
            logprint("shader_catalog", "In file '%s' line %d, Attempted to include a non-shader file at '%s'!\n", temp_c_string(shader->full_path), line_number, temp_c_string(relative_include_path));
            return false;
        }

        // We find in the catalog table instead of using catalog_find because
        // we don't want to trigger the included file to be called by reload_asset() if it
        // hasn't been loaded. The include system here is just so that we can reduce on
        // the amount of copy-pasting from shader to shader.
        auto [included_shader_asset, found_include] = table_find(&catalog->table, included_shader_name);

        if (!found_include)
        {
            logprint("shader_catalog", "In file '%s' line %d, Couldn't find a shader in the Shader_Catalog table for the include file at '%s'!\n", temp_c_string(shader->full_path), line_number, temp_c_string(relative_include_path));
            return false;
        }

        // Note in the included shader that it has us depends on it.
        // This is so that it can reload us too when it changes.
        array_add_if_unique(&included_shader_asset->shaders_that_include_me, shader);
    }

    auto include_full_path = join(2, path_excluding_file_name, relative_include_path);

    auto [other_shader_content, success] = read_entire_file(include_full_path);
    if (!success)
    {
        logprint("shader_catalog", "In file '%s' line %d, couldn't read the include file at path '%s'!\n", temp_c_string(shader->full_path), line_number, temp_c_string(include_full_path));
        return false;
    }

    defer { free_string(&other_shader_content); };

    append(builder, other_shader_content);

    return true;
}

String process_shader_text(String data, Shader *shader, Shader_Catalog *catalog)
{
    auto mark =  get_temporary_storage_mark();
    defer { set_temporary_storage_mark(mark); };

    String_Builder builder;

    i32 line_number = 0;
    while (true)
    {
        auto [line, found] = consume_next_line(&data);
        if (!found) break;

        bool skip_line = false; // For example, we will not put the include line into the builder.
    
        line_number += 1;

        eat_spaces(&line);
        // eat_trailing_spaces(&line);

        auto directive = is_our_custom_directive(line);
        if (directive)
        {
            auto [command, rhs] = break_by_spaces(directive);

            if (command == String("NoDepthWrite"))
            {
                shader->depth_write = false;
            }
            else if (command == String("NoBlend"))
            {
                shader->alpha_blend = false;
            }
            else if (command == String("NoDepthTest"))
            {
                shader->depth_test = false;
            }
            else if (command == String("DiffuseTextureClamped"))
            {
                shader->diffuse_texture_wraps = false;
            }
            else if (command == String("include"))
            {
                auto included = parse_shader_include(rhs, shader, catalog, &builder, line_number);
                skip_line = included;
            }
            else
            {
                logprint("shader_catalog", "In file '%s' line %d, found an unexpected directive '%s'!\n", temp_c_string(shader->full_path), line_number, temp_c_string(directive));
                assert(0); // Should we assert here?
            }
        }

        if (!skip_line) append(&builder, line);
        append(&builder, String("\n"));
    }

    auto processed = builder_to_string(&builder);
    return processed;
}

void reload_asset(Shader_Catalog *catalog, Shader *shader)
{
    auto [original_data, success] = read_entire_file(shader->full_path);

    if (!success)
    {
        logprint("shader_catalog", "Unable to load shader '%s' from file '%s'.\n",
                temp_c_string(shader->name), temp_c_string(shader->full_path));
        return;
    }

    if (!shader->is_header)
    {
        String processed_data = process_shader_text(original_data, shader, catalog);
        free_string(&original_data); // Free original data after processing

        success = load_shader_from_memory(processed_data, shader);
        free_string(&processed_data); // Free processed data after loaded to memory

        if (!success)
        {
            fprintf(stderr, "Shader '%s' loaded but failed to compile.\n", temp_c_string(shader->name));
            return;
        }
    }

    //
    // After we load ourselves, we also reload the shaders that depend on us.
    //
    for (auto other : shader->shaders_that_include_me)
    {
        // printf("Reload dependent shader '%s'!\n", temp_c_string(other->name));
        reload_asset(catalog, other);
    }
}
