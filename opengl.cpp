// @Important: REMEMBER THAT GLM USES COLUMN MAJOR ORDER FOR MATRICES
// So, M[4][1] means the first row of the last column

#include "opengl.h"
#include "draw.h"

#define xxx (void*)

// @Note: Not doing anything with these at the moment
bool in_windowed_mode = true;
bool ignore_resizes = false; 
bool should_vsync = true;

GLFWwindow *glfw_window;

// These will be defined by the user in init_window_and_gl()
i32 render_target_width;
i32 render_target_height;

i32 shadow_map_width  = 0;
i32 shadow_map_height = 0;
Texture_Map *shadow_map_buffer = NULL;
Texture_Map *shadow_map_depth  = NULL;

Texture_Map *the_depth_buffer = NULL;
Texture_Map *the_back_buffer = NULL;
Texture_Map *the_offscreen_buffer = NULL;
Texture_Map *the_ldr_buffer = NULL; // This is used an a resolved buffer for the offscreen_buffer when it is multisampled. @Incomplete: Try and coerce the multisampled version and the non-multisampled version.
i32 XXX_the_offscreen_buffer_width  = 1920;
i32 XXX_the_offscreen_buffer_height = 1080;

Matrix4 view_to_proj_matrix;
Matrix4 world_to_view_matrix;
Matrix4 object_to_world_matrix;
Matrix4 object_to_proj_matrix;

Matrix4 object_to_shadow_map_matrix;
Matrix4 world_to_shadow_map_matrix;

Shader *current_shader;

GLuint immediate_vbo;
GLuint immediate_vbo_indices;
GLuint opengl_is_stupid_vao;

const u32    MAX_IMMEDIATE_VERTICES = 2400;
Vertex_XCNUU immediate_vertices[MAX_IMMEDIATE_VERTICES];
u32          num_immediate_vertices = 0;

const u32 OFFSET_position      = 0;
const u32 OFFSET_color_scale   = 12;
const u32 OFFSET_normal        = 16;
const u32 OFFSET_uv0           = 28;
const u32 OFFSET_uv1           = 36;
const u32 OFFSET_lightmap_uv   = 36;
const u32 OFFSET_blend_weights = 44;
const u32 OFFSET_blend_indices = 60;

bool vertex_format_set_to_XCNUU = false;

bool multisampling    = true;
u32  num_multisamples = 4;

//
// End of variables definitions

// #define DumpGLErrors(tag) _dump_gl_errors(tag, __FUNCTION__, __LINE__, __FILE__)
bool _dump_gl_errors(const char *tag, const char *func, long line, const char *file)
{
    bool result = false;

    if (!glfwExtensionSupported("GL_ARB_debug_output"))
    {
        fprintf(stderr, "GL version does not support GL_ARB_debug_output...");
        return false;
    }

    while (true)
    {
        GLuint source, type, id;
        GLint length;

        constexpr u32 BUFFER_SIZE = 4096;
        char buffer[BUFFER_SIZE];

        GLenum severity;

        if (glGetDebugMessageLogARB(1, sizeof(buffer), &source, &type, &id, &severity, &length, buffer))
        {
            // GL_DEBUG_TYPE_ERROR_ARB
            // GL_DEBUG_SEVERITY_HIGH_ARB
            // GL_DEBUG_SEVERITY_MEDIUM_ARB

            if ((severity == GL_DEBUG_SEVERITY_MEDIUM_ARB) || (severity == GL_DEBUG_SEVERITY_HIGH_ARB))
            {
                fprintf(stderr, "[%s at %s:%ld] %s\n", tag, file, line, buffer);
                result = true;
            }
        }
        else
        {
            break;
        }
    }

    return result;
}

void DumpShaderInfoLog(GLuint shader, String name)
{
    constexpr u32 BUFFER_SIZE = 4096;
    SArr<u8> buffer = NewArray<u8>(BUFFER_SIZE); // @Speed:
    defer { my_free(&buffer); };

    buffer.data[0] = 0;

    GLsizei length_result = 0;
    glGetShaderInfoLog(shader, BUFFER_SIZE, &length_result, (GLchar*)buffer.data);

    if (length_result > 0)
    {
        String s;
        s.count = length_result;
        s.data  = buffer.data;
        printf("ShaderInfoLog for %s : %s\n", temp_c_string(name), temp_c_string(s));
    }
}

void DumpProgramInfoLog(GLuint program, String name)
{
    constexpr u32 BUFFER_SIZE = 4096;
    SArr<u8> buffer = NewArray<u8>(BUFFER_SIZE);
    defer { my_free(&buffer); };

    buffer.data[0] = 0;

    GLsizei length_result = 0;
    glGetProgramInfoLog(program, BUFFER_SIZE, &length_result, (GLchar*)buffer.data);

    if (length_result > 0)
    {
        String s;
        s.count = length_result;
        s.data  = buffer.data;
        printf("ProgramInfoLog for %s:\n%s\n\n", temp_c_string(name), temp_c_string(s));
    }
}

// @Cleanup: @Cutnpaste from set_vertex_format_to_XCNUU, this is only temporary before
// we figure out the render pipeline.
void set_vertex_format_to_XCNUUS(Shader *shader)
{
    u32 stride = sizeof(Vertex_XCNUUS);

    if (shader->position_loc != -1)
    {
        DumpGLErrors("position - beforehand abc");
        glEnableVertexAttribArray(shader->position_loc);
        DumpGLErrors("position - mid");
        glVertexAttribPointer(shader->position_loc, 3, GL_FLOAT, false, stride, xxx OFFSET_position);
        DumpGLErrors("position - after");
    }

    if (shader->color_scale_loc != -1)
    {
        glVertexAttribPointer(shader->color_scale_loc, 4, GL_UNSIGNED_BYTE, true, stride, xxx OFFSET_color_scale);
        glEnableVertexAttribArray(shader->color_scale_loc);
        DumpGLErrors("colors");
    }

    if (shader->normal_loc != -1)
    {
        glVertexAttribPointer(shader->normal_loc, 3, GL_FLOAT, false, stride, xxx OFFSET_normal);
        glEnableVertexAttribArray(shader->normal_loc);
        DumpGLErrors("normal");
    }

    if (shader->uv_0_loc != -1)
    {
        glVertexAttribPointer(shader->uv_0_loc, 2, GL_FLOAT, false, stride, xxx OFFSET_uv0);
        glEnableVertexAttribArray(shader->uv_0_loc);
        DumpGLErrors("uv0");
    }

    if (shader->lightmap_uv_loc != -1)
    {
        glVertexAttribPointer(shader->lightmap_uv_loc, 2, GL_FLOAT, false, stride, xxx OFFSET_lightmap_uv);
        glEnableVertexAttribArray(shader->lightmap_uv_loc);
        DumpGLErrors("Lightmap uv");
    }

    if (shader->blend_weights_loc != -1)
    {
        glVertexAttribPointer(shader->blend_weights_loc, 4, GL_FLOAT, true, stride, xxx OFFSET_blend_weights);
        glEnableVertexAttribArray(shader->blend_weights_loc);
        DumpGLErrors("Blend weights");
    }

    if (shader->blend_indices_loc != -1)
    {
        glVertexAttribIPointer(shader->blend_indices_loc, 4, GL_UNSIGNED_BYTE, stride, xxx OFFSET_blend_indices);
        glEnableVertexAttribArray(shader->blend_indices_loc);
        DumpGLErrors("Blend indices");
    }
}

void set_vertex_format_to_XCNUU(Shader *shader)
{
    vertex_format_set_to_XCNUU = true;

    u32 stride = sizeof(Vertex_XCNUU);

    if (shader->position_loc != -1)
    {
        DumpGLErrors("position - beforehand abc");
        // AttrayAtrib is GL 4.5:         glEnableVertexArrayAttrib(opengl_is_stupid_vao, shader->position_loc)
        // printf("position_loc: %d\n", shader->position_loc);
        glEnableVertexAttribArray(shader->position_loc);
        DumpGLErrors("position - mid");
        glVertexAttribPointer(shader->position_loc, 3, GL_FLOAT, false, stride, xxx OFFSET_position);
        DumpGLErrors("position - after");
    }

    DumpGLErrors("position - VertexAttribArray");

    if (shader->color_scale_loc != -1)
    {
        // printf("color_loc: %d\n", shader->color_scale_loc);
        glVertexAttribPointer(shader->color_scale_loc, 4, GL_UNSIGNED_BYTE, true, stride, xxx OFFSET_color_scale);
        glEnableVertexAttribArray(shader->color_scale_loc);
        DumpGLErrors("colors");
    }

    if (shader->normal_loc != -1)
    {
        // printf("normal_loc: %d\n", shader->normal_loc);
        glVertexAttribPointer(shader->normal_loc, 3, GL_FLOAT, false, stride, xxx OFFSET_normal);
        glEnableVertexAttribArray(shader->normal_loc);
        DumpGLErrors("normal");
    }

    if (shader->uv_0_loc != -1)
    {
        // printf("uv_0_loc: %d\n", shader->uv_0_loc);
        glVertexAttribPointer(shader->uv_0_loc, 2, GL_FLOAT, false, stride, xxx OFFSET_uv0);
        glEnableVertexAttribArray(shader->uv_0_loc);
        DumpGLErrors("uv0");
    }

    if (shader->uv_1_loc != -1)
    {
        // printf("uv_1_loc: %d\n", shader->uv_1_loc);
        glVertexAttribPointer(shader->uv_1_loc, 2, GL_FLOAT, false, stride, xxx OFFSET_uv1);
        glEnableVertexAttribArray(shader->uv_1_loc);
        DumpGLErrors("uv1");
    }

    // Disabling these because if they exists, they may interfere with the other vertex attributes.
    if (shader->blend_weights_loc != -1) glDisableVertexAttribArray(shader->blend_weights_loc);
    if (shader->blend_indices_loc != -1) glDisableVertexAttribArray(shader->blend_indices_loc);
}

u32 argb_color(Vector4 color)
{
    u32 ix = (u32)(color.x * 255.0);
    u32 iy = (u32)(color.y * 255.0);
    u32 iz = (u32)(color.z * 255.0);
    u32 ia = (u32)(color.w * 255.0);

    return (ia << 24) | (ix << 16) | (iy << 8) | (iz << 0);
}

u32 argb_color(Vector3 color)
{
    u32 ix = (u32)(color.x * 255.0);
    u32 iy = (u32)(color.y * 255.0);
    u32 iz = (u32)(color.z * 255.0);

    return 0xff000000 | (ix << 16) | (iy << 8) | (iz << 0);
}

Vector4 float_color(u32 c)
{
    Vector4 result;

    result.x = ((c >> 16) & 0xff) / 255.0;
    result.y = ((c >> 8)  & 0xff) / 255.0;
    result.z = ((c >> 0)  & 0xff) / 255.0;
    result.w = ((c >> 24) & 0xff) / 255.0;

    return result;
}

my_pair<i32, i32> get_mouse_pointer_position(GLFWwindow *window, bool right_handed)
{
    f64 query_x, query_y;
    glfwGetCursorPos(window, &query_x, &query_y);

    i32 x = (i32)(floor(query_x));
    i32 y = (i32)(floor(query_y));

    if (right_handed)
    {
        i32 window_width, window_height;
        glfwGetWindowSize(window, &window_width, &window_height);
        y = window_height - y;
    }

    return {x, y};
}

// Use this when you want the mouse pointer position with respect to the letter-boxed window.
my_pair<i32, i32> render_target_mouse_pointer_position(GLFWwindow *window, bool right_handed)
{
    auto [mouse_x, mouse_y] = get_mouse_pointer_position(window, right_handed);

    auto offset_x = static_cast<i32>((the_back_buffer->width - the_offscreen_buffer->width) / 2.0f);
    auto offset_y = static_cast<i32>((the_back_buffer->height - the_offscreen_buffer->height) / 2.0f);

    mouse_x -= offset_x;
    mouse_y -= offset_y;

    return {mouse_x, mouse_y};
}

void immediate_begin()
{
    immediate_flush();
}

void immediate_flush()
{
    if (!num_immediate_vertices) return;

    Shader *shader = current_shader;

    if (!shader)
    {
        if (num_immediate_vertices)
        {
            fprintf(stderr, "Tried to flush when no shader was set.\n");
            exit(1);
        }
        num_immediate_vertices = 0;
        return;
    }

    Vertex_XCNUU *v = immediate_vertices;
    u32 count = num_immediate_vertices;

    glBindVertexArray(opengl_is_stupid_vao);
    glBindBuffer(GL_ARRAY_BUFFER, immediate_vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    glBufferData(GL_ARRAY_BUFFER, sizeof(v[0]) * count, immediate_vertices, GL_STREAM_DRAW);

    // @Temporary: change it later, but for now format everything to XCNUU format
    if (true) // if (!vertex_format_set_to_XCNUU)
    {
        set_vertex_format_to_XCNUU(shader);
    }

    if (true)
    {
        glDrawArrays(GL_TRIANGLES, 0, count);
    }
    else // If want to use the element array buffer
    {
        u16 *indices = (u16*)my_alloc(sizeof(u16) * count);

        for (int i = 0; i < count; ++i)
            indices[i] = i;

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, immediate_vbo_indices);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(u16) * count, indices, GL_STREAM_DRAW);
        glDrawElements(GL_TRIANGLES, count, GL_UNSIGNED_SHORT, NULL);

        my_free(indices);
    }

    num_immediate_vertices = 0;
}

inline Vertex_XCNUU *immediate_vertex_ptr(i32 index)
{
    return immediate_vertices + index;
}

void put_vertex(Vertex_XCNUU *v, Vector2 p, u32 scale_color, f32 uv_u, f32 uv_v)
{
    v->position.x = p.x;
    v->position.y = p.y;
    v->position.z = 0;
    v->color_scale = abgr(scale_color);
    v->normal = Vector3(0, 0, 1);
    v->uv0.x = uv_u;
    v->uv0.y = uv_v;
    v->uv1 = Vector2(0, 0);
}

void put_vertex(Vertex_XCNUU *v, Vector3 p, u32 scale_color, f32 uv_u, f32 uv_v)
{
    v->position.x = p.x;
    v->position.y = p.y;
    v->position.z = p.z;
    v->color_scale = abgr(scale_color);
    v->normal = Vector3(0, 0, 1);
    v->uv0.x = uv_u;
    v->uv0.y = uv_v;
    v->uv1 = Vector2(0, 0);
}

// put_vertex with normal
void put_vertex(Vertex_XCNUU *v, Vector2 p, u32 scale_color, Vector3 normal, f32 uv_u, f32 uv_v)
{
    v->position.x = p.x;
    v->position.y = p.y;
    v->position.z = 0;
    v->color_scale = abgr(scale_color);
    v->normal = normal;
    v->uv0.x = uv_u;
    v->uv0.y = uv_v;
    v->uv1 = Vector2(0, 0);
}

void put_vertex(Vertex_XCNUU *v, Vector3 p, u32 scale_color, Vector3 normal, f32 uv_u, f32 uv_v)
{
    v->position.x = p.x;
    v->position.y = p.y;
    v->position.z = p.z;
    v->color_scale = abgr(scale_color);
    v->normal = normal;
    v->uv0.x = uv_u;
    v->uv0.y = uv_v;
    v->uv1 = Vector2(0, 0);
}

void immediate_vertex(Vector3 position, u32 color_scale, Vector3 normal, Vector2 uv)
{
    if (num_immediate_vertices == MAX_IMMEDIATE_VERTICES)
    {
        immediate_flush();
    }

    Vertex_XCNUU *v = immediate_vertex_ptr(num_immediate_vertices);

    v->position = position;
    v->color_scale = abgr(color_scale);
    v->normal = normal;
    v->uv0 = uv;
    v->uv1 = Vector2(0, 0);

    num_immediate_vertices += 1;
}

void immediate_triangle(Vector2 p0, Vector2 p1, Vector2 p2, u32 color)
{
    if (num_immediate_vertices > MAX_IMMEDIATE_VERTICES - 3)
    {
        immediate_flush();
    }

    Vertex_XCNUU *v = immediate_vertex_ptr(num_immediate_vertices);

    put_vertex(&v[0], p0,  color,  0, 0);
    put_vertex(&v[1], p1,  color,  1, 0);
    put_vertex(&v[2], p2,  color,  1, 1);

    num_immediate_vertices += 3;
}

void immediate_triangle(Vector2 p0, Vector2 p1, Vector2 p2, u32 c0, u32 c1, u32 c2)
{
    if (num_immediate_vertices > MAX_IMMEDIATE_VERTICES - 3)
    {
        immediate_flush();
    }

    Vertex_XCNUU *v = immediate_vertex_ptr(num_immediate_vertices);

    put_vertex(&v[0], p0,  c0,  0, 0);
    put_vertex(&v[1], p1,  c1,  1, 0);
    put_vertex(&v[2], p2,  c2,  1, 1);

    num_immediate_vertices += 3;
}

void immediate_quad(Vector2 p0, Vector2 p1, Vector2 p2, Vector2 p3, u32 color)
{
    if (num_immediate_vertices > MAX_IMMEDIATE_VERTICES - 6)
    {
        immediate_flush();
    }

    Vertex_XCNUU *v = immediate_vertex_ptr(num_immediate_vertices);

    put_vertex(&v[0], p0,  color,  0, 0);
    put_vertex(&v[1], p1,  color,  1, 0);
    put_vertex(&v[2], p2,  color,  1, 1);

    put_vertex(&v[3], p0,  color,  0, 0);
    put_vertex(&v[4], p2,  color,  1, 1);
    put_vertex(&v[5], p3,  color,  0, 1);

    num_immediate_vertices += 6;
}

void immediate_quad(Vector2 p0, Vector2 p1, Vector2 p2, Vector2 p3,
                    Vector4 c0, Vector4 c1, Vector4 c2, Vector4 c3)
{
    if (num_immediate_vertices > MAX_IMMEDIATE_VERTICES - 6)
    {
        immediate_flush();
    }

    Vertex_XCNUU *v = immediate_vertex_ptr(num_immediate_vertices);

    put_vertex(&v[0], p0,  argb_color(c0),  0, 0);
    put_vertex(&v[1], p1,  argb_color(c1),  1, 0);
    put_vertex(&v[2], p2,  argb_color(c2),  1, 1);

    put_vertex(&v[3], p0,  argb_color(c0),  0, 0);
    put_vertex(&v[4], p2,  argb_color(c2),  1, 1);
    put_vertex(&v[5], p3,  argb_color(c3),  0, 1);

    num_immediate_vertices += 6;
}

void immediate_quad(Vector2 p0, Vector2 p1, Vector2 p2, Vector2 p3, Vector3 normal, u32 color)
{
    if (num_immediate_vertices > MAX_IMMEDIATE_VERTICES - 6)
    {
        immediate_flush();
    }

    Vertex_XCNUU *v = immediate_vertex_ptr(num_immediate_vertices);

    put_vertex(&v[0], p0,  color,  normal,  0, 0);
    put_vertex(&v[1], p1,  color,  normal,  1, 0);
    put_vertex(&v[2], p2,  color,  normal,  1, 1);

    put_vertex(&v[3], p0,  color,  normal,  0, 0);
    put_vertex(&v[4], p2,  color,  normal,  1, 1);
    put_vertex(&v[5], p3,  color,  normal,  0, 1);

    num_immediate_vertices += 6;
}

/*
void immediate_quad(Vector3 p0, Vector3 p1, Vector3 p2, Vector3 p3,
                    Vector2 uv0, Vector2 uv1, Vector2 uv2, Vector2 uv3,
                    Vector3 normal, u32 multiply_color)
{
    if (num_immediate_vertices > MAX_IMMEDIATE_VERTICES - 6)
    {
        immediate_flush();
    }

    Vertex_XCNUU *v = immediate_vertex_ptr(num_immediate_vertices);

    put_vertex(&v[0], p0, multiply_color, normal, uv0.x, uv0.y);
    put_vertex(&v[1], p1, multiply_color, normal, uv1.x, uv1.y);
    put_vertex(&v[2], p2, multiply_color, normal, uv2.x, uv2.y);

    put_vertex(&v[3], p0, multiply_color, normal, uv0.x, uv0.y);
    put_vertex(&v[4], p2, multiply_color, normal, uv2.x, uv2.y);
    put_vertex(&v[5], p3, multiply_color, normal, uv3.x, uv3.y);

    num_immediate_vertices += 6;
}
*/

void immediate_quad(Vector3 p0,  Vector3 p1,  Vector3 p2,  Vector3 p3,
                    Vector2 uv0, Vector2 uv1, Vector2 uv2, Vector2 uv3,
                    u32 multiply_color)
{
    if (num_immediate_vertices > MAX_IMMEDIATE_VERTICES - 6)
    {
        immediate_flush();
    }

    Vertex_XCNUU *v = immediate_vertex_ptr(num_immediate_vertices);

    put_vertex(&v[0], p0, multiply_color, uv0.x, uv0.y);
    put_vertex(&v[1], p1, multiply_color, uv1.x, uv1.y);
    put_vertex(&v[2], p2, multiply_color, uv2.x, uv2.y);

    put_vertex(&v[3], p0, multiply_color, uv0.x, uv0.y);
    put_vertex(&v[4], p2, multiply_color, uv2.x, uv2.y);
    put_vertex(&v[5], p3, multiply_color, uv3.x, uv3.y);

    num_immediate_vertices += 6;
}

void immediate_quad(Vector3 p0,  Vector3 p1,  Vector3 p2,  Vector3 p3,
                    u32 multiply_color)
{
    if (num_immediate_vertices > MAX_IMMEDIATE_VERTICES - 6)
    {
        immediate_flush();
    }

    Vertex_XCNUU *v = immediate_vertex_ptr(num_immediate_vertices);

    put_vertex(&v[0], p0, multiply_color, 0, 0);
    put_vertex(&v[1], p1, multiply_color, 1, 0);
    put_vertex(&v[2], p2, multiply_color, 1, 1);

    put_vertex(&v[3], p0, multiply_color, 0, 0);
    put_vertex(&v[4], p2, multiply_color, 1, 1);
    put_vertex(&v[5], p3, multiply_color, 0, 1);

    num_immediate_vertices += 6;
}

void immediate_quad(Vector3 p0,  Vector3 p1,  Vector3 p2,  Vector3 p3,
                    Vector3 normal, u32 multiply_color)
{
    if (num_immediate_vertices > MAX_IMMEDIATE_VERTICES - 6)
    {
        immediate_flush();
    }

    Vertex_XCNUU *v = immediate_vertex_ptr(num_immediate_vertices);

    put_vertex(&v[0], p0, multiply_color, normal, 0, 0);
    put_vertex(&v[1], p1, multiply_color, normal, 1, 0);
    put_vertex(&v[2], p2, multiply_color, normal, 1, 1);

    put_vertex(&v[3], p0, multiply_color, normal, 0, 0);
    put_vertex(&v[4], p2, multiply_color, normal, 1, 1);
    put_vertex(&v[5], p3, multiply_color, normal, 0, 1);

    num_immediate_vertices += 6;
}

void immediate_quad(Vector2 p0, Vector2 p1, Vector2 p2, Vector2 p3,
                    Vector2 uv0, Vector2 uv1, Vector2 uv2, Vector2 uv3,
                    u32 multiply_color)
{
    if (num_immediate_vertices > MAX_IMMEDIATE_VERTICES - 6)
    {
        immediate_flush();
    }

    Vertex_XCNUU *v = immediate_vertex_ptr(num_immediate_vertices);

    put_vertex(&v[0], p0, multiply_color, uv0.x, uv0.y);
    put_vertex(&v[1], p1, multiply_color, uv1.x, uv1.y);
    put_vertex(&v[2], p2, multiply_color, uv2.x, uv2.y);

    put_vertex(&v[3], p0, multiply_color, uv0.x, uv0.y);
    put_vertex(&v[4], p2, multiply_color, uv2.x, uv2.y);
    put_vertex(&v[5], p3, multiply_color, uv3.x, uv3.y);

    num_immediate_vertices += 6;
}

void immediate_letter_quad(Font_Quad q, Vector4 color)
{
    if (num_immediate_vertices > MAX_IMMEDIATE_VERTICES - 6)
    {
        immediate_flush();
    }

    Vertex_XCNUU *v = immediate_vertex_ptr(num_immediate_vertices);

    auto p1 = q.p0 + (q.p1 - q.p0) / 3.0f;
    auto p2 = q.p3 + (q.p2 - q.p3) / 3.0f;

    auto multiply_color = argb_color(color);

    put_vertex(&v[0], q.p0, multiply_color, q.u0, q.v0);
    put_vertex(&v[1],   p1, multiply_color, q.u1, q.v0);
    put_vertex(&v[2],   p2, multiply_color, q.u1, q.v1);

    put_vertex(&v[3], q.p0, multiply_color, q.u0, q.v0);
    put_vertex(&v[4],   p2, multiply_color, q.u1, q.v1);
    put_vertex(&v[5], q.p3, multiply_color, q.u0, q.v1);

    num_immediate_vertices += 6;
}

// Bitmap *copy_framebuffer_into_bitmap()
// {
//     auto width = render_target_width;
//     auto height = render_target_height;

//     auto result = New<Bitmap>();
//     result->format = Texture_Format::ARGB8888;

//     constexpr auto bpp = 3;
//     auto num_bytes = width * height * bpp;
//     result->data   = my_alloc(num_bytes);
//     result->width  = width;
//     result->height = height;
//     result->num_mipmap_levels = 1;
//     result->length_in_bytes = num_bytes;

//     glReadPixels(0, 0, (GLsizei)width, (GLsizei)height, GL_RGB, GL_UNSIGNED_BYTE, result->data);
//     return result;
// }

void refresh_transform()
{
    if (num_immediate_vertices)
    {
        immediate_flush();
    }

    Matrix4 m = view_to_proj_matrix * (world_to_view_matrix * object_to_world_matrix);
    object_to_proj_matrix = m;

    if (current_render_type == Render_Type::SHADOW_MAP)
    {
        object_to_shadow_map_matrix = world_to_shadow_map_matrix * object_to_world_matrix;

        if (current_shader)
        {
            // printf("ots:\n");
            // print_cmaj_as_rmaj(object_to_shadow_map_matrix);

            // Because glm uses the transposed thing like Opengl, we tell is GL_FALSE for not transposing back :(
            glUniformMatrix4fv(current_shader->transform_loc, 1, GL_FALSE, &object_to_shadow_map_matrix[0][0]);
        }
    }
    else
    {
        // assert(current_render_type == Render_Type::MAIN_VIEW);

        if (current_shader)
        {
            // Because glm uses the transposed thing like Opengl, we tell is GL_FALSE for not transposing back :(
            glUniformMatrix4fv(current_shader->transform_loc, 1, GL_FALSE, &object_to_proj_matrix[0][0]);
        }
    }
}

void apply_2d_shader(Shader *shader)
{
    set_shader(shader);
}

void set_shader(Shader *shader)
{
    if (shader == current_shader) return;
    if (current_shader) immediate_flush();

    current_shader = shader;

    // If no shader, we bail.
    if (!shader)
    {
        glUseProgram(0);
        DumpGLErrors("glUseProgram");
        return;
    }

    // Using the shader program.
    if (shader->program >= 0)
    {
        // printf("       Set_shader, program %s\n", temp_c_string(shader->name));
        DumpGLErrors("shader->program");
        glUseProgram(shader->program);
        refresh_transform();
    }

    // Alpha blend stuff.
    if (shader->alpha_blend)
    {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }
    else glDisable(GL_BLEND);

    // Depth testing stuff.
    if (shader->depth_test)
    {
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
    }
    else glDisable(GL_DEPTH_TEST);

    // Depth write.
    if (shader->depth_write)
    {
        glDepthMask(GL_TRUE);
    }
    else glDepthMask(GL_FALSE);

    // Backface culling.
    if (shader->backface_cull)
    {
        glEnable(GL_CULL_FACE);
        glFrontFace(GL_CCW);
    }
    else glDisable(GL_CULL_FACE);
}

void size_color_target(Texture_Map *map, bool do_hdr)
{
    GLenum texture_target = GL_TEXTURE_2D;
    if (multisampling)
    {
        texture_target = GL_TEXTURE_2D_MULTISAMPLE;
    }

    glBindTexture(texture_target, map->id);
    if (do_hdr)
    {
        if (multisampling)
        {
            glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, num_multisamples, GL_RGBA16F,
                                    (GLsizei)map->width, (GLsizei)map->height, GL_TRUE);
            DumpGLErrors("multisampling1");
        }
        else
        {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F,
                         (GLsizei)map->width, (GLsizei)map->height, 0,
                         GL_RGBA, GL_HALF_FLOAT, NULL);
        }
    }
    else
    {
        if (multisampling)
        {
            glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, num_multisamples, GL_RGBA8,
                                    (GLsizei)map->width, (GLsizei)map->height, 1);
            DumpGLErrors("multisampling2");
        }
        else
        {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
                         (GLsizei)map->width, (GLsizei)map->height, 0,
                         GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        }
    }

    glTexParameteri(texture_target, GL_TEXTURE_MAX_LEVEL, 0); // This has no mipmaps
}

void size_depth_target(Texture_Map *map)
{
    if (multisampling)
    {
        glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, map->id);
        glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, num_multisamples, GL_DEPTH_COMPONENT,
                                (GLsizei)map->width, (GLsizei)map->height, GL_TRUE);
        glTexParameteri(GL_TEXTURE_2D_MULTISAMPLE, GL_TEXTURE_MAX_LEVEL, 0); // This has no mipmaps
        DumpGLErrors("depth target multisampled");
    }
    else
    {
        glBindTexture(GL_TEXTURE_2D, map->id);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT,
                     (GLsizei)map->width, (GLsizei)map->height, 0,
                     GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0); // This has no mipmaps
        DumpGLErrors("depth target non-multisampled");
    }
}

my_pair<Texture_Map*, Texture_Map*> create_texture_rendertarget(i32 width, i32 height, bool do_depth_target, bool do_hdr)
{
    auto map = New<Texture_Map>(false);
    init_texture_map(map);
    map->width  = width;
    map->height = height;
    map->dirty  = false;
    // map->format = TEXTURE_FORMAT_SYSTEM_SPECIFIC;

    Texture_Map *depth_map = NULL;
    if (do_depth_target)
    {
        depth_map = New<Texture_Map>(false);
        init_texture_map(depth_map);
    }

    glGenFramebuffers(1, (GLuint*)&map->fbo_id);
    if (depth_map) glGenFramebuffers(1, (GLuint*)&depth_map->fbo_id);

    // Get the currently bound texture so we can restore it.
    GLuint current_map_id;
    glActiveTexture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, (GLint*)&current_map_id);

    //
    // Color target:
    //
    glGenTextures(1, &map->id);
    size_color_target(map, do_hdr);

    //
    // Depth target:
    //
    if (depth_map)
    {
        depth_map->width  = width;
        depth_map->height = height;

        glGenTextures(1, &depth_map->id);
        size_depth_target(depth_map);
    }

    // Restore previous texture.
    glBindTexture(GL_TEXTURE_2D, current_map_id);

    return {map, depth_map};
}

void set_render_target(u32 index, Texture_Map *map, Texture_Map *depth_map)
{
    // Asserting with 0 because we currently only have 1 render target.
    assert((index == 0));

    if ((index == 0) && (map == the_back_buffer))
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, map->width, map->height);
    }
    else
    {
        glBindFramebuffer(GL_FRAMEBUFFER, map->fbo_id);

        if (multisampling)
        {
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + index, GL_TEXTURE_2D_MULTISAMPLE, map->id, 0);
            glCheckFramebufferStatus(GL_FRAMEBUFFER);
            DumpGLErrors("check multisampled color_map");
        }
        else
        {
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + index, GL_TEXTURE_2D, map->id, 0);
            DumpGLErrors("check non-multisampled color_map");
        }

        if (depth_map)
        {
            if (multisampling)
            {
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D_MULTISAMPLE, depth_map->id, 0);
                DumpGLErrors("check multisampled depth_map");
            }
            else
            {
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth_map->id, 0);
                DumpGLErrors("check depth_map");
            }

            glEnable(GL_DEPTH_TEST);
            glDepthMask(GL_TRUE);
        }

        if (multisampling)
        {
            // @Hack @Hack @Hack: We are hardcoding to render to the ldr buffer when we do multisampling. This is bad.
            glBindFramebuffer(GL_FRAMEBUFFER, the_ldr_buffer->fbo_id);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + index, GL_TEXTURE_2D, the_ldr_buffer->id, 0);

            glBindFramebuffer(GL_READ_FRAMEBUFFER, map->fbo_id);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, the_ldr_buffer->fbo_id);
            glBlitFramebuffer(0, 0, map->width, map->height, 0, 0, the_ldr_buffer->width, the_ldr_buffer->height, GL_COLOR_BUFFER_BIT, GL_NEAREST);

            glViewport(0, 0, map->width, map->height);
            glBindFramebuffer(GL_FRAMEBUFFER, map->fbo_id); // Bind the actual offscreen buffer back so that we can draw stuff to here.
        }
        else
        {
            GLenum draw_buffers[] = {GL_COLOR_ATTACHMENT0};
            glDrawBuffers(1, draw_buffers);
            glViewport(0, 0, map->width, map->height);
        }
    }

    render_target_width = map->width;
    render_target_height = map->height;
}

Texture_Map *last_applied_diffuse_map = NULL;
Texture_Map *last_applied_mask_map = NULL;

/* @Incomplete: Need this:
Texture_Map *create_texture(Bitmap *data)
{
    auto map = New<Texture_Map>();
    map->width  = data->width;
    map->height = data->height;
    map->data   = data;

    // update_texture(map);

    return map;
}
*/

void update_texture_from_bitmap(Texture_Map *map, Bitmap *bitmap)
{
    assert(map->num_mipmap_levels >= 1);

    if (!map->id || (map->id == 0xffffffff)) // @Cleanup: Decide on which one to use as UNINITIALIZE.
    {
        glGenTextures(1, &map->id);
        // logprint("update_texture_from_bitmap", "Creating a new texture with id %d for '%s'.\n", map->id, temp_c_string(map->name));
    }

    if (!bitmap)
    {
        logprint("update_texture_from_bitmap", "Tried to set a bitmap to texture '%s'?!?!\n", temp_c_string(map->name));
        return;
    }

    glBindTexture(GL_TEXTURE_2D, map->id);

    assert(bitmap->width && bitmap->height);
    map->width  = bitmap->width;
    map->height = bitmap->height;

    auto texture_info = get_ogl_format(map->format, map->is_srgb);

    auto bitmap_data = bitmap->data;
    if (!texture_info.compressed)
    {
        glPixelStorei(GL_UNPACK_ALIGNMENT, texture_info.alignment);
        glTexImage2D(GL_TEXTURE_2D, 0, texture_info.dest_format,
                     map->width, map->height,
                     0, texture_info.src_format, texture_info.src_type, xxx bitmap_data);
        glGenerateMipmap(GL_TEXTURE_2D);
    }
    else
    {
        auto block_size = texture_info.block_size;

        assert(block_size); // In case I forget to set it above.
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1); // Set alignment to 1, just in case...

        auto width  = map->width;
        auto height = map->height;

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, map->num_mipmap_levels - 1); // @Important: The MAX_LEVEL thing here is the mipmap levels count and it is 0-based indexed, so we do a subtract by 1.

        u32 offset = 0;
        for (u32 mip_level = 0; mip_level < map->num_mipmap_levels; ++mip_level)
        {
            if (!width && !height) break;

            u32 image_size = ((width + 3)/4) * ((height + 3)/4) * block_size; // Size in bytes, round to nearest power of 2.

            glCompressedTexImage2D(GL_TEXTURE_2D, mip_level, texture_info.dest_format, width, height, 0, image_size, bitmap_data + offset);

            offset += image_size;
            width  /= 2;
            height /= 2;

            if (width  < 1) width  = 1;
            if (height < 1) height = 1;
        }
    }
}

/*
void destroy_texture(Texture_Map *map)
{
    if (map->id)
    {
        // @Incomplete: @Fixme
        // glDeleteTextures(1, &map->id);
        map->id = 0;
    }

    map->width  = 0;
    map->height = 0;
}
*/

void set_texture(String texture_name, Texture_Map *map)
{
    auto shader = current_shader;
    if (!shader) return;

    if (!map)
    {
        logprint("set_texture", "Tried to pass a NULL texture to set_texture!\n");
        return;
    }

    if (map->dirty)
    {
        map->dirty = false;
        assert(0); // set_texture will not take care of reloading bitmaps for the textures, the caller will do that.
        // update_texture(map);
    }

    auto wrapping = false;
    auto flow_map = false; // @Todo: this is for water rendering

    GLint loc          = -1;
    GLint texture_unit = 0; // @Note: This corresponds to the layout (binding = ...) in the shader.

    // @Hack
    if (equal(texture_name, String("diffuse_texture")))
    {
        if (map == last_applied_diffuse_map) return;
        last_applied_diffuse_map = map;
        loc                      = shader->diffuse_texture_loc;
        texture_unit             = 0;
        wrapping                 = true;
    }
    else if (equal(texture_name, String("lightmap_texture")))
    {
        texture_unit = 1;
        loc          = shader->lightmap_texture_loc;
        wrapping     = true;
    }
    else if (equal(texture_name, String("blend_texture")))
    {
        texture_unit = 2;
        loc          = shader->blend_texture_loc;
        wrapping     = true;
    }
    else if (texture_name == String("normal_map_texture"))
    {
        texture_unit = 3;
        loc          = shader->normal_map_texture_loc;
        wrapping     = true;
    }
    else
    {
        auto c_name  = temp_c_string(texture_name);
        loc          = glGetUniformLocation(shader->program, (char*)c_name);
        wrapping     = false;
        texture_unit = 4;
    }

    immediate_flush(); // Flush *after* permitting early-out

    if (loc < 0) return;

    glUniform1i(loc, texture_unit);
    glActiveTexture((GLenum)(GL_TEXTURE0 + texture_unit));

    if (map) glBindTexture(GL_TEXTURE_2D, map->id);
    else     glBindTexture(GL_TEXTURE_2D, 0);

    if (loc == shader->diffuse_texture_loc)
    {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

        if (!shader->diffuse_texture_wraps)
        {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        }

        if (shader->textures_point_sample)
        {
            if (map->num_mipmap_levels > 1)
            {
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST_MIPMAP_LINEAR);
            }
            else
            {
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            }
        }
        else
        {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        }
    }
    // @Temporary: should diffuse_texture_wraps be handled here??
    else if (wrapping && shader->diffuse_texture_wraps) // use diffuse_texture_wraps here cause I think it is needed 
    {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }
    else if (flow_map)
    {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }
    else
    {
        // @Hack: This helps with the shadow map texture not getting repeated to places where
        // we should not do shadows. We should think of moving it somewhere else but I don't know man.
        if (map == shadow_map_depth)
        {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
            
            auto border_color = Vector4(1, 1, 1, 1);
            glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, (f32*)&border_color);

            return;
        }

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
}

void set_float4_parameter(const char *name, Vector4 _value)
{
    Shader *shader = current_shader;

    if (!shader) return;

    Vector4 value = _value;
    u32 loc = glGetUniformLocation(shader->program, name);

    if (loc >= 0) glUniform4fv(loc, 1, (f32*)&value);
    else fprintf(stderr, "Unable to set variable '%s' on shader '%s'\n", name, temp_c_string(shader->name));
}

#undef xxx
