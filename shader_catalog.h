#pragma once

#include "common.h"

#include <glad/glad.h>
#include "catalog.h"

struct Shader
{
    String                name;
    String           full_path;

    // ID for VS/FS shaders and the shader program
    GLuint       vertex_shader;
    GLuint     fragment_shader;
    GLuint             program;

    // Location in GPU's memory for setting uniforms
    GLint         position_loc;
    GLint      color_scale_loc;
    GLint           normal_loc;
    GLint             uv_0_loc;
    GLint             uv_1_loc;
    GLint      lightmap_uv_loc;

    GLint    blend_weights_loc;
    GLint    blend_indices_loc;

    // Uniforms
    GLint        transform_loc;
    GLint   blend_matrices_loc;

    // Texture samplers
    GLint      diffuse_texture_loc; // For albedo.
    GLint     lightmap_texture_loc;
    GLint        blend_texture_loc;
    GLint   normal_map_texture_loc; // For normal mapping.
    GLint           sampler_cursor;

    // These are related to textures
    bool diffuse_texture_wraps; // True by default
    bool textures_point_sample; // True by default

    bool           alpha_blend; // True by default
    bool            depth_test; // True by default
    bool           depth_write; // True by default
    bool         backface_cull = true;

    // @Note: For Catalog
    bool                loaded = false;

    // For hotreload dependency:
    bool is_header = false; // Headers don't really compile into a shader program.
    RArr<Shader*> shaders_that_include_me;
};

using Shader_Catalog = Catalog<Shader>;

void    init_shader_catalog(Shader_Catalog *catalog);
void    deinit_shader_catalog(Shader_Catalog *catalog);
Shader *make_placeholder(Shader_Catalog *catalog, String short_name, String full_name);
void    reload_asset(Shader_Catalog *catalog, Shader *shader);
