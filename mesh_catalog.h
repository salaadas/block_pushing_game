#pragma once

#include "common.h"
#include "catalog.h"
#include "texture_catalog.h"

struct Triangle_List_Info
{
    i32 material_index; // Which material to use to render these triangles?
    i32 num_indices;    // How many vertices are there in the list? (must be a multiple of 3)
    i32 first_index;    // What is the index of the beginning of this list? (like an offset)

    Texture_Map *map; // :DeprecateMe, use the map_2024 in Render_Material.
};

// struct Parameterization_Settings
// {
//     f32  texel_per_unit;        // This is the number of texels in a meter (1 / texel_width).
//     i32  packing_quality = -1;
//     bool seam_fitting;
//     bool custom_parameterization;
//     bool enable_vertex_lightmap;
//     bool force_vertex_lightmap;
//     f32  crease_fitting_weight; // .25
//     i32  revision;
// };

enum class Triangle_Mesh_Flags : u32
{
    Instanced       = 0x01, 
    Do_Not_Pack     = 0x02,
    No_Z_Only       = 0x04,
    Manual_LODs     = 0x08,
    No_Collision    = 0x10,
    Collision_Only  = 0x20,     // Mesh only has collision info.
    Do_Not_Optimize = 0x40,     // Temporary mesh, do not optimize.
    HDR_Colors      = 0x80,
    Blend_In_Alpha  = 0x100,
    Shared_Lightmap = 0x200,    // All LODs share the same lightmap.
    Cluster_LOD     = 0x400,
};

enum class Material_Flags : u32
{
    Dynamic_Substitute                = 0x01, // This means the material will not really use the texture map indicated, but something provided by the entity itself.
    Casts_Shadow                      = 0x02,
    Lightmaped                        = 0x04,
    Remove_During_Reduction           = 0x10,
    Do_Not_Use_When_Computing_Normals = 0x20,
    Detail                            = 0x40
};

enum class Material_Type
{
    Standard,
    Foliage,
    Lake,
    Reflective,
    Translucent,
    Blended,
    Decal,
    Shadow_Only,
    Vegetation,
};

struct Frame3
{
    Vector3 tangent;
    Vector3 bitangent;
    Vector3 normal;
};

struct Bounding_Box // Not used.
{
    Vector3 min;
    Vector3 max;
};

struct Render_Material
{
    String  texture_map_name; // This name will only be used when loading the model for the first time to attain the texture map, which will be stored in 'map' of Triangle List Info. :DeprecateMe will not be needed anymore.
    Vector4 color; // @Cleanup: Rename to base_color or albedo color!
    u32     flags; // Using Material_Flags @Incomplete:

    Texture_Map *albedo_2024 = NULL;
    // Texture_Map *normal_2024 = NULL;
};

struct Vertex_Blend_Info_Piece
{
    i32 matrix_index  = 0;
    f32 matrix_weight = 0;
};

#define MAX_MATRICES_PER_VERTEX 4
struct Vertex_Blend_Info
{
    i32 num_matrices = 0;
    // Because I want it to be allocated along with the
    // vertex blend info, I'm making it a regular array here.
    Vertex_Blend_Info_Piece pieces[MAX_MATRICES_PER_VERTEX];
};

struct Skeleton_Node_Info
{
    String  name;
    Matrix4 rest_object_space_to_object_space;
};

struct Skeleton_Info
{
    SArr<Skeleton_Node_Info> skeleton_node_info;
    SArr<Vertex_Blend_Info>  vertex_blend_info;
};

// // Runtime skeleton data.
// struct Skeleton
// {
//     i32 num_bones = 0;
//     SArr<String> bone_names;                         // size of this is num_bones.
//     SArr<Matrix4> rest_object_space_to_object_space; // size of this is num_bones.
// };

struct Triangle_Mesh
{
    u32 flags; // Using Triangle_Mesh_Flags

    GLuint vertex_vbo;
    GLuint index_vbo;

    // Catalog asset stuff
    String name;
    String full_path;

    // Imported vertex data:
    SArr<Vector3> vertices;
    SArr<Vector2> uvs;
    SArr<Frame3>  vertex_frames;
    SArr<Vector4> colors;
    SArr<i32>     canonical_vertex_map;

    // Computed vertex data:
    // SArr<Vector2> lightmap_uvs;
    // SArr<Vector4> lightmap_colors;

    SArr<Vector4> texture_colors;

    // Bounds:
    Vector3  approximate_bounding_box_p0;
    Vector3  approximate_bounding_box_p1;
    // Vector2  lightmap_uv_extents;

    // Triangle data:
    SArr<i32> index_array;

    // // Edge smoothing data:
    // SArr<i32> edge_smoothing_info;

    SArr<Triangle_List_Info> triangle_list_info;
    SArr<Render_Material>    material_info;

    Skeleton_Info *skeleton_info = NULL;  // @Incomplete:
    SArr<i32>     vertex_to_skeleton_info_map;

    // LOD data:
    // Triangle_Mesh      *highest_detail_lod; // NULL if LOD not being used
    // SArr<Triangle_Mesh*> lods;

    // Parameterization_Settings parameterization_settings;
    // i32 vertex_color_revision;

    bool loaded = false; // For catalog
    bool dirty  = true;
};

using Mesh_Catalog = Catalog<Triangle_Mesh>;

void init_mesh_catalog(Mesh_Catalog *catalog);
Triangle_Mesh *make_placeholder(Mesh_Catalog *catalog, String short_name, String full_name);
void reload_asset(Mesh_Catalog *catalog, Triangle_Mesh *mesh);

bool load_gltf_model_2024(Triangle_Mesh *triangle_mesh);
bool load_fbx_model(Triangle_Mesh *mesh);
