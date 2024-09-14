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
    shader->lightmap_uv_loc   = 5;
    shader->blend_weights_loc = -1;
    shader->blend_indices_loc = -1;

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
    // @Fixme: Loading from the folder names 'my_name' for now
    catalog->base.my_name = String("shaders");
    array_add(&catalog->base.extensions, String("gl"));

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
    Shader *shader    = New<Shader>(false);
    init_shader(shader);
    shader->name      = copy_string(short_name);
    shader->full_path = copy_string(full_path);

    return shader;
}

bool load_shader_from_memory(String shader_text, String short_name, Shader *shader)
{
    // @Note: Shader must be zero-terminated

    shader->vertex_shader   = glCreateShader(GL_VERTEX_SHADER);
    shader->fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);

    if (shader_text.count)
    {
        char *c_s_text = (char*)to_c_string(shader_text);
        defer { my_free(c_s_text); };

        String vert_string = sprint(String("#version 430 core\n#define VERTEX_SHADER\n#define COMM out\n%s"),
                                    c_s_text);
        String frag_string = sprint(String("#version 430 core\n#define FRAGMENT_SHADER\n#define COMM in\n%s"),
                                    c_s_text);

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
#define shader_get_uniform(s, loc) \
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

        // newline();

#undef shader_get_uniform

        return true;
    }

    return false;
}

// @Todo: handle include file(s) in shader
// Remember to free the returned value
String process_shader_text(String data, Shader *shader)
{
    String processed = copy_string(data);

    i64 index = 0;

    while (isspace(processed[index]))
        index += 1;

    while (index < (processed.count - 1))
    {
        if (processed[index] == '/' && processed[index + 1] == '/')
        {
            index += 2;

            while (isspace(processed[index]))
                index += 1;

            if (processed[index] == '@' && processed[index + 1] == '@')
            {
                index += 2;

                String temp;
                u8 data[64];
                temp.data  = data;
                temp.count = 0;

                while (!isspace(processed[index]))
                {
                    temp.data[temp.count] = processed[index];
                    temp.count += 1;
                    index += 1;
                }

                // @Hack: @Temporary:
                if (temp == String("NoDepthWrite"))
                {
                    // printf("------------> no depth writeee\n");
                    shader->depth_write = false;
                }
                else if (temp == String("NoBlend"))
                {
                    shader->alpha_blend = false;
                }
                else if (temp == String("NoDepthTest"))
                {
                    // printf("------------> no depth test\n");
                    shader->depth_test = false;
                }
                else if (temp == String("DiffuseTextureClamped"))
                {
                    // printf("------------> diffuse texture clamped\n");
                    shader->diffuse_texture_wraps = false;
                }
                else
                {
                    printf("Unkown attribute: '%s'\n", temp_c_string(temp));
                }
            }
        }
        else
        {
            break;
        }

        while (isspace(processed[index]))
            index += 1;
    }

    return processed;
}

void reload_asset(Shader_Catalog *catalog, Shader *shader)
{
    auto [original_data, success] = read_entire_file(shader->full_path);

    // printf("Loading %s\n", temp_c_string(shader->name));

    if (!success)
    {
        fprintf(stderr, "Unable to load shader '%s' from file '%s'.\n",
                temp_c_string(shader->name), temp_c_string(shader->full_path));
        return;
    }

    String processed_data = process_shader_text(original_data, shader);
    free_string(&original_data); // Free original data after processing

    success = load_shader_from_memory(processed_data, shader->name, shader);
    free_string(&processed_data); // Free processed data after loaded to memory

    if (!success)
    {
        fprintf(stderr, "Shader '%s' loaded but failed to compile.\n", temp_c_string(shader->name));
        return;
    }
}
