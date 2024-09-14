#pragma once

#include "common.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "shader_catalog.h"
#include "texture.h"
#include "font.h"

extern bool in_windowed_mode;
extern bool ignore_resizes;
extern bool should_vsync;

extern GLFWwindow *glfw_window;

extern i32 render_target_width;
extern i32 render_target_height;

extern Texture_Map *the_depth_buffer;
extern Texture_Map *the_back_buffer;
extern Texture_Map *the_offscreen_buffer;
extern i32          XXX_the_offscreen_buffer_width; // :DeprecateMe
extern i32          XXX_the_offscreen_buffer_height; // :DeprecateMe

extern Matrix4 view_to_proj_matrix;
extern Matrix4 world_to_view_matrix;
extern Matrix4 object_to_world_matrix;
// The above 3 transformations can be performed in a single linear transformation below
extern Matrix4 object_to_proj_matrix;

struct Vertex_XCNUU
{
    Vector3 position;
    u32     color_scale;
    Vector3 normal;
    Vector2 uv0;
    Vector2 uv1;
};

struct Vertex_XCNUUS
{
    Vector3 position;
    u32     color_scale;
    Vector3 normal;
    Vector2 uv0;
    Vector2 lightmap_uv;

    Vector4 blend_weights;
    u32     blend_indices;
};

// @Hack @Temporary: probably not a good idea to hardcode it this way...
extern const u32 OFFSET_position;
extern const u32 OFFSET_color_scale;
extern const u32 OFFSET_normal;
extern const u32 OFFSET_uv0;
extern const u32 OFFSET_uv1;
extern const u32 OFFSET_lightmap_uv;
extern const u32 OFFSET_blend_weights;
extern const u32 OFFSET_blend_indices;

extern bool vertex_format_set_to_XCNUU;

extern Shader *current_shader;

extern GLuint immediate_vbo;
extern GLuint immediate_vbo_indices;

extern GLuint opengl_is_stupid_vao;

const extern u32    MAX_IMMEMDIATE_VERTICES;
extern Vertex_XCNUU immediate_vertices[];
extern u32          num_immediate_vertices;

void immediate_begin();
void immediate_flush();

void put_vertex(Vertex_XCNUU *v, Vector2 p, u32 scale_color, f32 uv_u, f32 uv_v);
void put_vertex(Vertex_XCNUU *v, Vector3 p, u32 scale_color, f32 uv_u, f32 uv_v);
void put_vertex(Vertex_XCNUU *v, Vector2 p, u32 scale_color, Vector3 normal, f32 uv_u, f32 uv_v);

void immediate_vertex(Vector3 position, u32 color_scale, Vector3 normal, Vector2 uv);

void immediate_quad(Vector2 p0, Vector2 p1, Vector2 p2, Vector2 p3, u32 color);
void immediate_quad(Vector2 p0, Vector2 p1, Vector2 p2, Vector2 p3, Vector3 normal, u32 color);
void immediate_quad(Vector2 p0, Vector2 p1, Vector2 p2, Vector2 p3,
                    Vector4 c0, Vector4 c1, Vector4 c2, Vector4 c3);

void immediate_letter_quad(Font_Quad q, Vector4 color);

/*
void immediate_quad(Vector3 p0, Vector3 p1, Vector3 p2, Vector3 p3,
                    Vector2 uv0, Vector2 uv1, Vector2 uv2, Vector2 uv3,
                    Vector3 normal, u32 multiply_color);
*/
void immediate_quad(Vector2 p0,  Vector2 p1,  Vector2 p2,  Vector2 p3,
                    Vector2 uv0, Vector2 uv1, Vector2 uv2, Vector2 uv3,
                    u32 multiply_color);

void immediate_quad(Vector3 p0,  Vector3 p1,  Vector3 p2,  Vector3 p3,
                    u32 multiply_color);
void immediate_quad(Vector3 p0,  Vector3 p1,  Vector3 p2,  Vector3 p3,
                    Vector2 uv0, Vector2 uv1, Vector2 uv2, Vector2 uv3,
                    u32 multiply_color);

void immediate_quad(Vector3 p0,  Vector3 p1,  Vector3 p2,  Vector3 p3,
                    Vector3 normal, u32 multiply_color);

void immediate_triangle(Vector2 p0, Vector2 p1, Vector2 p2, u32 color);
void immediate_triangle(Vector2 p0, Vector2 p1, Vector2 p2, u32 c0, u32 c1, u32 c2);

void set_shader(Shader *shader);
// void set_texture(String texture_name, Texture_Map *map);

void refresh_transform();

inline
u32 abgr(u32 argb)
{
    u32 a = (argb & 0xff000000);
    u32 r = (argb & 0x00ff0000) >> 16;
    u32 g = (argb & 0x0000ff00) >> 8;
    u32 b = (argb & 0x000000ff);

    return a | (b << 16) | (g << 8) | r;
}
u32 argb_color(Vector4 color);
u32 argb_color(Vector3 color);
Vector4 float_color(u32 c);

void DumpShaderInfoLog(GLuint shader, String name);
void DumpProgramInfoLog(GLuint program, String name);
bool _dump_gl_errors(const char *tag, const char *func, long line, const char *file);
#define DumpGLErrors(tag) _dump_gl_errors(tag, __FUNCTION__, __LINE__, __FILE__)

Texture_Map *create_texture(Bitmap *data);
void set_texture(String texture_name, Texture_Map *map);
void update_texture(Texture_Map *map);

my_pair<Texture_Map*, Texture_Map*> create_texture_rendertarget(i32 width, i32 height,
                                                                bool do_depth_target = false,
                                                                bool do_hdr = false);
void set_render_target(u32 index, Texture_Map *map, Texture_Map *depth_map = NULL);
void size_color_target(Texture_Map *map, bool do_hdr);
void size_depth_target(Texture_Map *map);

my_pair<i32, i32> get_mouse_pointer_position(GLFWwindow *window, bool right_handed);
my_pair<i32, i32> render_target_mouse_pointer_position(GLFWwindow *window, bool right_handed);

void set_vertex_format_to_XCNUU(Shader *shader);
