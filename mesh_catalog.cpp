// @Incomplete: Put a check in Pass 1 to see if there is a model that requires multiple primitives per triangle list!
// @Incomplete: Put a check in Pass 1 to see if there is a model that requires multiple primitives per triangle list!
// @Incomplete: Put a check in Pass 1 to see if there is a model that requires multiple primitives per triangle list!

#include "mesh_catalog.h"

/*
#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
*/

#include <stb_image.h>

#include "file_utils.h"
#include "opengl.h"
#include "main.h"

#include <glm/gtc/type_ptr.hpp> // for glm::make_mat4

#include "time_info.h"

/*
void assimp_mat4_to_glm(aiMatrix4x4 src, Matrix4 *dest)
{
    (*dest)[0][0]=src.a1;  (*dest)[1][0]=src.b1;  (*dest)[2][0]=src.c1;  (*dest)[3][0]=src.d1;
    (*dest)[0][1]=src.a2;  (*dest)[1][1]=src.b2;  (*dest)[2][1]=src.c2;  (*dest)[3][1]=src.d2;
    (*dest)[0][2]=src.a3;  (*dest)[1][2]=src.b3;  (*dest)[2][2]=src.c3;  (*dest)[3][2]=src.d3;
    (*dest)[0][3]=src.a4;  (*dest)[1][3]=src.b4;  (*dest)[2][3]=src.c4;  (*dest)[3][3]=src.d4;
}

void glm_mat4_to_assimp(Matrix4 src, aiMatrix4x4 *dest)
{
    dest->a1=src[0][0];  dest->a2=src[1][0];  dest->a3=src[2][0];  dest->a4=src[3][0];
    dest->b1=src[0][1];  dest->b2=src[1][1];  dest->b3=src[2][1];  dest->b4=src[3][1];
    dest->c1=src[0][2];  dest->c2=src[1][2];  dest->c3=src[2][2];  dest->c4=src[3][2];
    dest->d1=src[0][3];  dest->d2=src[1][3];  dest->d3=src[2][3];  dest->d4=src[3][3];
}
*/

void init_bounding_box(Bounding_Box *box) // @Incomplete: Not using bounding box.
{
    constexpr auto inf = 1e10f;
    box->min = Vector3(+inf, +inf, +inf);
    box->max = Vector3(-inf, -inf, -inf);
}

void expand(Bounding_Box *box, Vector3 v)
{
    if (v.x < box->min.x) box->min.x = v.x;
    if (v.y < box->min.y) box->min.y = v.y;
    if (v.z < box->min.z) box->min.z = v.z;

    if (v.x > box->max.x) box->max.x = v.x;
    if (v.y > box->max.y) box->max.y = v.y;
    if (v.z > box->max.z) box->max.z = v.z;
}

inline
void allocate_texture_colors(Triangle_Mesh *triangle_mesh)
{
    assert(triangle_mesh->texture_colors.count == 0);
    
    auto num_vertices = triangle_mesh->vertices.count;
    assert(num_vertices != 0);
    triangle_mesh->texture_colors = NewArray<Vector4>(num_vertices);
}

inline
void allocate_materials(Triangle_Mesh *triangle_mesh, i32 n)
{
    assert(triangle_mesh->material_info.count == 0);
    triangle_mesh->material_info = NewArray<Render_Material>(n);
}

inline
void allocate_triangle_list_info(Triangle_Mesh *triangle_mesh, i32 n)
{
    assert(triangle_mesh->triangle_list_info.count == 0);
    triangle_mesh->triangle_list_info = NewArray<Triangle_List_Info>(n);
}

inline
void allocate_color_data(Triangle_Mesh *triangle_mesh)
{
    assert(triangle_mesh->colors.count == 0);

    auto num_vertices = triangle_mesh->vertices.count;
    assert(num_vertices != 0);

    triangle_mesh->colors = NewArray<Vector4>(num_vertices);
}

inline
void allocate_geometry(Triangle_Mesh *triangle_mesh, i32 num_vertices, i32 face_count)
{
    assert(triangle_mesh->vertices.count             == 0 &&
           triangle_mesh->uvs.count                  == 0 &&
           triangle_mesh->vertex_frames.count        == 0 // &&
           /* triangle_mesh->canonical_vertex_map.count == 0 */);

    triangle_mesh->vertices             = NewArray<Vector3>(num_vertices);
    triangle_mesh->uvs                  = NewArray<Vector2>(num_vertices);
    triangle_mesh->vertex_frames        = NewArray<Frame3>(num_vertices);
    // triangle_mesh->canonical_vertex_map = NewArray<i32>(num_vertices);
    triangle_mesh->index_array          = NewArray<i32>(face_count * 3);
}

/*
void process_assimp_model(aiScene *model, Triangle_Mesh *triangle_mesh, String model_path_relative_to_exe) // :DeprecateMe
{
    Bounding_Box bounding_box;
    init_bounding_box(&bounding_box);

    i32 total_vertices = 0;
    i32 total_faces    = 0;
    i32 total_bones    = 0;
    // The number of meshes corresponds to the number of triangle list infos in our triangle mesh.
    // This is because our definition of mesh is different from that of Assimp. With Assimp,
    // a model can contain multiple meshes, but with us, everything is stored under one giant
    // mesh. But when we render, we use the triangle lists to query for the index of the vertices
    // of each mesh and we can render it with different textures and materials.
    i32 total_triangle_list_info = model->mNumMeshes;

    //
    // Pass 1: Get total number of vertices by traversing through the assimp model
    //
    bool use_colors   = true;
    bool use_textures = true;

    for (i32 assimp_mesh_i = 0; assimp_mesh_i < total_triangle_list_info; ++assimp_mesh_i)
    {
        auto assimp_mesh = model->mMeshes[assimp_mesh_i];

        total_vertices += assimp_mesh->mNumVertices;
        total_faces    += assimp_mesh->mNumFaces;
        total_bones    += assimp_mesh->mNumBones;

        if (assimp_mesh->mColors[0] == NULL) use_colors = false;
        if (assimp_mesh->mTextureCoords[0] == NULL) use_textures = false;
    }

    printf("Total mesh:     %d\n", total_triangle_list_info);
    printf("Total faces:    %d\n", total_faces);
    printf("Total vertices: %d\n", total_vertices);
    printf("Total bones:    %d\n", total_bones);

    // @Incomplete: Not parsing the bones of the mesh.
    // @Incomplete: Also, we are not allocating the array to store the bones.

    // Allocate the geometry arrays and the color/texture arrays

    if (!(total_vertices && total_faces))
    {
        fprintf(stderr, "ERROR: Model '%s' contains no vertices or faces, so cannot read.\n",
                temp_c_string(triangle_mesh->name));
        return;
    }

    if (!(total_triangle_list_info))
    {
        fprintf(stderr, "ERROR: Model '%s' contains no mesh components/parts, so cannot read.\n",
                temp_c_string(triangle_mesh->name));
        return;
    }

    allocate_geometry(triangle_mesh, total_vertices, total_faces);
    allocate_triangle_list_info(triangle_mesh, total_triangle_list_info);

    if (use_colors)   allocate_color_data(triangle_mesh);
    if (use_textures) allocate_texture_colors(triangle_mesh);

    auto num_materials = model->mNumMaterials;
    if (num_materials) allocate_materials(triangle_mesh, num_materials);

    //
    // Pass 2: Go through the assimp model and insert the data into the triangle list
    // 
    i32 current_index = 0;
    i32 vertices_previous_lists = 0;
    for (i32 assimp_mesh_i = 0; assimp_mesh_i < total_triangle_list_info; ++assimp_mesh_i)
    {
        auto assimp_mesh = model->mMeshes[assimp_mesh_i];

        //
        // Process all the vertices for this assimp mesh first.
        //
        auto vertices_this_list = assimp_mesh->mNumVertices;
        for (i32 vertex_i = 0; vertex_i < vertices_this_list; ++vertex_i)
        {
            auto assimp_vertex  = assimp_mesh->mVertices[vertex_i];
            Vector3 mesh_vertex = Vector3(assimp_vertex.x, assimp_vertex.y, assimp_vertex.z);
            expand(&bounding_box, mesh_vertex);

            Vector3 vertex = mesh_vertex;
            Vector2 uv;
            Frame3  vertex_frame;
            Vector4 color;

            if (assimp_mesh->mNormals != NULL)
            {
                vertex_frame.normal.x = assimp_mesh->mNormals[vertex_i].x;
                vertex_frame.normal.y = assimp_mesh->mNormals[vertex_i].y;
                vertex_frame.normal.z = assimp_mesh->mNormals[vertex_i].z;
            }

            // Only get the first texture coordinate and color
            if (assimp_mesh->mTextureCoords[0] != NULL)
            {
                uv.x = assimp_mesh->mTextureCoords[0][vertex_i].x;
                uv.y = assimp_mesh->mTextureCoords[0][vertex_i].y;
            }

            if (assimp_mesh->mColors[0] != NULL)
            {
                color.x = assimp_mesh->mColors[0][vertex_i].r;
                color.y = assimp_mesh->mColors[0][vertex_i].g;
                color.z = assimp_mesh->mColors[0][vertex_i].b;
                color.w = assimp_mesh->mColors[0][vertex_i].a;
            }

            if (assimp_mesh->mTangents != NULL)
            {
                vertex_frame.tangent.x = assimp_mesh->mTangents[vertex_i].x;
                vertex_frame.tangent.y = assimp_mesh->mTangents[vertex_i].y;
                vertex_frame.tangent.z = assimp_mesh->mTangents[vertex_i].z;

                vertex_frame.bitangent.x = assimp_mesh->mBitangents[vertex_i].x;
                vertex_frame.bitangent.y = assimp_mesh->mBitangents[vertex_i].y;
                vertex_frame.bitangent.z = assimp_mesh->mBitangents[vertex_i].z;
            }

            auto v_i = vertex_i + vertices_previous_lists;
            triangle_mesh->vertices     [v_i] = vertex;
            triangle_mesh->uvs          [v_i] = uv;
            triangle_mesh->vertex_frames[v_i] = vertex_frame;

            if (use_colors)
            {
                triangle_mesh->colors[v_i] = color;
            }
            if (use_textures)
            {
                // @Temporary:
                triangle_mesh->texture_colors[v_i] = Vector4(1, 1, 1, 1);
            }
        }

        //
        // Then, we store the index data and the triangle list for the assimp mesh.
        //
        auto faces_for_this_list = assimp_mesh->mNumFaces;

        auto current_list_info         = &triangle_mesh->triangle_list_info[assimp_mesh_i];
        current_list_info->num_indices = 3 * faces_for_this_list;
        current_list_info->first_index = current_index;

        // @Temporary:
        current_list_info->material_index = -1;
        current_list_info->map = NULL;

        for (i32 face_i = 0; face_i < faces_for_this_list; ++face_i)
        {
            auto face = assimp_mesh->mFaces[face_i];
            auto indices_count = face.mNumIndices;
            assert((indices_count == 3)); // Because we triangulated the mesh.

            for (i32 it = 0; it < indices_count; ++it)
            {
                auto vertex_index = face.mIndices[it];
                auto v_i = vertex_index + vertices_previous_lists;
                triangle_mesh->index_array[current_index] = v_i;

                current_index += 1;
            }
        }

        vertices_previous_lists += vertices_this_list;

        // //
        // // We then start to process the bones for this particular assimp mesh of the model.
        // //
        // {
        //     auto bones_for_this_mesh = assimp_mesh->mNumBones;

        //     if (assimp_mesh->mBones && (bones_for_this_mesh > 0)) // If we have any bones to process.
        //     {
        //         for (i32 bone_index = 0; bone_index < bones_for_this_mesh; ++bone_index)
        //         {
        //             auto assimp_bone = assimp_mesh->mBones[bone_index]; // This is a pointer to aiBone.
        //             auto bone_name   = assimp_bone->mName.data; // This char* name needs to be copied.

        //             auto total_weights_for_this_bone = assimp_bone->mNumWeights;
        //             for (i32 weight_index = 0; weight_index < total_weights_for_this_bone; ++weight_index) // For all the influence weights of this bone.
        //             {
        //                 auto vertex_weight = &assimp_bone->mWeights[weight_index];
        //                 auto vertex_id = vertex_weight->mVertexId; // What vertex is being influenced by this bone.
        //                 // This is the strength of the influence in the range of [0..1],
        //                 // The influence from all bones at one vertex amounts to 1, always.
        //                 auto weight = vertex_weight->mWeight;
        //             }

        //             // @Incomplete:
        //         }
        //     }
        // }

        //
        // Next, we load the materials to the triangle mesh
        //
        auto material_index = assimp_mesh->mMaterialIndex;
        current_list_info->material_index = material_index;

        auto assimp_material = model->mMaterials[material_index];

        auto assimp_texture_type   = aiTextureType_DIFFUSE;
        auto diffuse_texture_count = aiGetMaterialTextureCount(assimp_material, assimp_texture_type);

        auto diffuse_material = &triangle_mesh->material_info[material_index];
        diffuse_material->texture_map_name.count = 0;

        if (!diffuse_texture_count)
        {
            fprintf(stderr, "[triangle_mesh]: WARNING, MESH '%s' DOES NOT CONTAINS A DIFFUSE TEXTURE!\n", temp_c_string(triangle_mesh->name));
            current_list_info->map = NULL;
        }
        else
        {
            // @Note: This path is relative to the location of the model.
            aiString assimp_material_path;

            // @Temporary Getting the first diffuse texture because our renderer isn't sophisticated for now.
            auto success = aiGetMaterialTexture(assimp_material, assimp_texture_type, 0, &assimp_material_path);

            if (success != AI_SUCCESS)
            {
                fprintf(stderr, "[triangle_mesh]: Unable to get material diffuse texture of mesh, continuing...\n");
                goto set_triangle_mesh_color;
            }
            
            // @Incomplete: Before loading the embedded texture, we must check if the texture is in
            // the texture_catalog, if yes, load from there. Else, load the embedded texture and
            // put it inside the texture_catalog.
            auto assimp_embedded_texture = model->GetEmbeddedTexture(assimp_material_path.data);
            if (assimp_embedded_texture)
            {
                auto width  = assimp_embedded_texture->mWidth;
                auto height = assimp_embedded_texture->mHeight;

                if (height == 0) height = width;

                // Our policy for the embedded texture maps is that we don't add them to the texture catalog, unlike
                // the external textures. This is because we have mesh catalog that stores the meshes, so we don't
                // have to worry about loading the same embedded texture map twice since we will have catalog to
                // store meshes.
                // ^ @Fixme: This is wrong, we should store it in the texture catalog, because the same texture can be loaded more than once in a mesh, even if it is an embedded one.
                auto resulting_map = New<Texture_Map>(false);
                init_texture_map(resulting_map);

                i32 components;
                u8 *data = stbi_load_from_memory(reinterpret_cast<u8*>(assimp_embedded_texture->pcData),
                                                 width * height, &resulting_map->width, &resulting_map->height,
                                                 &components, 4);
                if (!data)
                {
                    fprintf(stderr, "FAILED to load embedded bitmap for mesh '%s'\n", temp_c_string(triangle_mesh->name));
                    goto set_triangle_mesh_color;
                }

                auto bitmap = New<Bitmap>(false);
                bitmap->width  = resulting_map->width;
                bitmap->height = resulting_map->height;
                bitmap->data   = data;
                bitmap->length_in_bytes = bitmap->width * bitmap->height * components;

                // @Temporary @Hack @Fixme @Incomplete
                if (components == 3)
                {
                    bitmap->format = Texture_Format::RGB888;
                }
                else
                {
                    bitmap->format = Texture_Format::ARGB8888;
                }

                resulting_map->data = bitmap;
                update_texture(resulting_map);

                current_list_info->map = resulting_map;
            }
            else
            {
                String material_path_relative_to_model = String(assimp_material_path.data);

                auto rel_model_path = model_path_relative_to_exe;
                auto index_of_last_slash = find_index_from_right(rel_model_path, '/');
                rel_model_path.count -= rel_model_path.count - index_of_last_slash;

                auto material_path_relative_to_exe = tprint(String("%s/%s"), temp_c_string(rel_model_path), temp_c_string(material_path_relative_to_model));

                String file_name = find_character_from_right(material_path_relative_to_model, '/');

                if (file_name.count == 0) file_name = material_path_relative_to_model;
                else advance(&file_name, 1);

                if (file_name.count <= 0)
                {
                    logprint("triangle_mesh", "Could not find the filename of the material path '%s'.\n", assimp_material_path.data);
                    goto set_triangle_mesh_color;
                }

                String name_without_extension = find_character_from_left(file_name, '.');
                name_without_extension.count -= 1;
                if (name_without_extension.count <= 0)
                {
                    logprint("triangle_mesh", "File name without extension is malformed '%s'.\n", assimp_material_path.data);
                    goto set_triangle_mesh_color;
                }

                // @Incomplete: If multiple models has the same name for the texture, we will load the wrong one.
                // @Incomplete: If multiple models has the same name for the texture, we will load the wrong one.
                // @Incomplete: If multiple models has the same name for the texture, we will load the wrong one.
                // @Incomplete: If multiple models has the same name for the texture, we will load the wrong one.
                // ^ @Fixme: Unless we fix the catalog_find to compare with the full name if the short name matches.
                // ^ @Fixme: Unless we fix the catalog_find to compare with the full name if the short name matches.
                diffuse_material->texture_map_name = copy_string(name_without_extension);
                my_register_loose_file<Texture_Map>(&texture_catalog.base, diffuse_material->texture_map_name, material_path_relative_to_exe);
            }
        }

    set_triangle_mesh_color:

        aiColor4D assimp_diffuse_color;
        auto success = aiGetMaterialColor(assimp_material, AI_MATKEY_COLOR_DIFFUSE, &assimp_diffuse_color);
        if (success != AI_SUCCESS)
        {
            fprintf(stderr, "[triangle_mesh]: Unable to get the material color for mesh, reverting to the color white...\n");
            diffuse_material->color = Vector4(1, 1, 1, 1);
        }

        diffuse_material->color = Vector4(assimp_diffuse_color.r, assimp_diffuse_color.g, assimp_diffuse_color.b, assimp_diffuse_color.a);
    }

    assert((current_index == triangle_mesh->index_array.count));

    triangle_mesh->approximate_bounding_box_p0   = bounding_box.min;
    triangle_mesh->approximate_bounding_box_p1   = bounding_box.max;
    triangle_mesh->approximate_bounding_box_p1.z = bounding_box.min.z;
}
*/

bool load_mesh_into_memory(Triangle_Mesh *mesh)
{
    assert((mesh->full_path));
    auto full_path   = mesh->full_path;
    auto c_path      = (char*)(temp_c_string(full_path));
    auto extension   = find_character_from_right(full_path, '.');

    if (!extension) return false; // No extension meaning that we don't know how to handle it.

    advance(&extension, 1);

    if ((extension == String("gltf")) || (extension == String("glb")))
    {
        auto success = load_gltf_model_2024(mesh);
        return success;
    }
    else
    {
        assert(0);
    }

    /* Assimp loading stuff
    auto supported_extension = aiIsExtensionSupported(c_extension);
    if (supported_extension == AI_FALSE)
    {
        logprint("mesh_catalog", "ERROR: Extension '%s' is not supported.\n", c_extension);
        return false;
    }

    aiScene *model = const_cast<aiScene*>(aiImportFile(c_path, aiProcessPreset_TargetRealtime_Quality));
    defer { aiReleaseImport(model); };

    if (!model)
    {
        logprint("mesh_catalog", "Failed to load the model at path '%s'.\n", c_path);
        return false;
    }

    process_assimp_model(model, mesh, full_path);
    */

    return false;
}

//
// Catalog stuff
//

// This is used in loading gltf materials' textures.
// The textures stored in here are either:
//     a) Loaded from the embedded model file, or
//     b) Retrieved using the texture catalog.
RArr<Texture_Map*> textures_lookup;

void init_mesh_catalog(Mesh_Catalog *catalog)
{
    catalog->base.my_name = String("Triangle Mesh");

    // array_add(&catalog->base.extensions, String("obj"));

    // array_add(&catalog->base.extensions, String("fbx"));
    array_add(&catalog->base.extensions, String("gltf"));
    array_add(&catalog->base.extensions, String("glb"));

    textures_lookup.allocator = {global_context.temporary_storage, __temporary_allocator};

    do_polymorphic_catalog_init(catalog);
}

Triangle_Mesh *make_placeholder(Mesh_Catalog *catalog, String short_name, String full_name)
{
    auto mesh       = New<Triangle_Mesh>();
    mesh->name      = copy_string(short_name);
    mesh->full_path = copy_string(full_name);
    mesh->dirty     = true;

    return mesh;
}

void free_resources(Triangle_Mesh *mesh)
{
    array_free(&mesh->vertices);
    array_free(&mesh->uvs);
    array_free(&mesh->vertex_frames);
    array_free(&mesh->colors);
    array_free(&mesh->texture_colors);

    array_free(&mesh->index_array);

    array_free(&mesh->triangle_list_info);
    array_free(&mesh->material_info);

    // Freeing the skeleton info.
    {
        auto info = mesh->skeleton_info;

        for (auto &node : info->skeleton_node_info)
        {
            free_string(&node.name);
        }

        array_free(&info->skeleton_node_info);
        array_free(&info->vertex_blend_info);

        my_free(info);
    }
}

inline
void make_vertex_buffer_non_skinned(Triangle_Mesh *mesh)
{
    auto count       = mesh->vertices.count;
    auto dest_buffer = NewArray<Vertex_XCNUU>(count);

    i64 it_index = 0;
    for (auto &dest : dest_buffer)
    {
        // Every mesh supposed to have a vertex, but what the heyy, consistency!!?!?!?
        if (mesh->vertices) dest.position = mesh->vertices[it_index];
        else                dest.position = Vector3(0, 0, 0);

        if (mesh->colors)
        {
            auto c = mesh->colors[it_index];
            c.w = 1.0f;

            dest.color_scale = argb_color(c);
        }
        else
        {
            dest.color_scale = 0xffffffff;
        }

        if (mesh->vertex_frames) dest.normal = mesh->vertex_frames[it_index].normal;
        else                     dest.normal = Vector3(1, 0, 0);

        if (mesh->uvs) dest.uv0 = mesh->uvs[it_index];
        else dest.uv0 = Vector2(0, 0);

        dest.uv1 = Vector2(0, 0);
            
        it_index += 1;
    }

    assert(dest_buffer.count);
    assert(mesh->index_array.count);

    glGenBuffers(1, &mesh->vertex_vbo);
    glGenBuffers(1, &mesh->index_vbo);
    DumpGLErrors("glGenBuffers for mesh's vertices");

    glBindBuffer(GL_ARRAY_BUFFER, mesh->vertex_vbo);
    DumpGLErrors("glBindBuffer for mesh's vertices");

    glBufferData(GL_ARRAY_BUFFER, sizeof(dest_buffer[0]) * count, dest_buffer.data, GL_STREAM_DRAW);
    DumpGLErrors("glBufferData for mesh's vertices");

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->index_vbo);
    DumpGLErrors("glBindBuffer for mesh's indices");

    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(mesh->index_array[0]) * mesh->index_array.count, mesh->index_array.data, GL_STREAM_DRAW);
    DumpGLErrors("glBufferData for mesh's indices");
}

//
// This is just a @Cutnpaste from the above. I'm not making this a polymorphic
// procedure because all of this XCNUU(S) format will go away later and so
// making 2 seperate procedure is also faster for compilation than one with some 
// compile time if's in it.
//
inline
void make_vertex_buffer_skinned(Triangle_Mesh *mesh)
{
    assert(mesh->skeleton_info);
    assert(mesh->skeleton_info->vertex_blend_info.count);

    auto count       = mesh->vertices.count;
    auto dest_buffer = NewArray<Vertex_XCNUUS>(count);

    i64 it_index = 0;
    for (auto &dest : dest_buffer)
    {
        // Every mesh supposed to have a vertex, but what the heyy, consistency!!?!?!?
        if (mesh->vertices) dest.position = mesh->vertices[it_index];
        else                dest.position = Vector3(0, 0, 0);

        if (mesh->colors)
        {
            auto c = mesh->colors[it_index];
            c.w = 1.0f;
            dest.color_scale = argb_color(c);
        }
        else
        {
            dest.color_scale = 0xffffffff;
        }

        if (mesh->vertex_frames) dest.normal = mesh->vertex_frames[it_index].normal;
        else                     dest.normal = Vector3(1, 0, 0);

        if (mesh->uvs) dest.uv0 = mesh->uvs[it_index];
        else           dest.uv0 = Vector2(0, 0);

        // This is the part that is different compared to the non_skinned version.
        dest.lightmap_uv = Vector2(0, 0); // @Incomplete: No lightmap data for now...

        u32     blend_indices = 0; // This gets unpacked as a vec4 of unsigned bytes in the shader.
        Vector4 blend_weights(0, 0, 0, 0);
        auto blend = &mesh->skeleton_info->vertex_blend_info[it_index];
        for (auto i = 0; i < blend->num_matrices; ++i) // @Speed:
        {
            auto piece = &blend->pieces[i];

            blend_indices   |= static_cast<u8>(piece->matrix_index) << ((i) * 8);
            blend_weights[i] = piece->matrix_weight;
        }

        dest.blend_weights = blend_weights;
        dest.blend_indices = blend_indices;
            
        it_index += 1;
    }

    assert(dest_buffer.count);
    assert(mesh->index_array.count);

    glGenBuffers(1, &mesh->vertex_vbo);
    glGenBuffers(1, &mesh->index_vbo);
    DumpGLErrors("glGenBuffers for mesh's vertices");

    glBindBuffer(GL_ARRAY_BUFFER, mesh->vertex_vbo);
    DumpGLErrors("glBindBuffer for mesh's vertices");

    glBufferData(GL_ARRAY_BUFFER, sizeof(dest_buffer[0]) * count, dest_buffer.data, GL_STREAM_DRAW);
    DumpGLErrors("glBufferData for mesh's vertices");

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->index_vbo);
    DumpGLErrors("glBindBuffer for mesh's indices");

    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(mesh->index_array[0]) * mesh->index_array.count, mesh->index_array.data, GL_STREAM_DRAW);
    DumpGLErrors("glBufferData for mesh's indices");
}

void reload_asset(Mesh_Catalog *catalog, Triangle_Mesh *mesh)
{
    // auto start_time = get_time();
    // printf("Started loading mesh '%s'\n", temp_c_string(mesh->name));

    if (mesh->loaded)
    {
        logprint("mesh_catalog", "Freeing the mesh '%s'!!!!!\n", temp_c_string(mesh->full_path));
        free_resources(mesh);
    }

    auto load_success = load_mesh_into_memory(mesh);
    if (!load_success)
    {
        logprint("mesh_catalog", "Was not able to load the mesh '%s' into memory!\n", temp_c_string(mesh->full_path));
        return;
    }

    // Make vertex buffer for the mesh with respect to its skinning data.
    if (mesh->skeleton_info) make_vertex_buffer_skinned(mesh);
    else                     make_vertex_buffer_non_skinned(mesh);

    // auto end_time = get_time() - start_time;
    // printf("Loaded the mesh '%s', took %f seconds!\n", temp_c_string(mesh->name), end_time);
    // newline();
}

#include <cgltf.h>

inline
cgltf_sampler *get_sampler_of_image(cgltf_data *parsed_gltf_data, i64 image_index, cgltf_image *gltf_image)
{
    auto probe_index = image_index;
    assert(probe_index < parsed_gltf_data->textures_count);

    do
    {
        auto gltf_texture = &parsed_gltf_data->textures[probe_index];
        if (gltf_texture->image == gltf_image)
        {
            auto sampler = gltf_texture->sampler;
            return sampler;
        }

        probe_index += 1;
        if (probe_index >= parsed_gltf_data->textures_count) probe_index = 0;
    }
    while (probe_index != image_index); // If we wrap around to where we start, we bail

    assert(0); // @Incomplete: Unhandled case.
    return NULL;
}

//
// Given an accessor (in GLTF, this is the main way to read data), we load the
// copy the content of that accessor (we are not allocating any data, just assigning
// the pointer elsewhere) to a given memory address.
// If the count or type address(es) is provided, we also store the count of the accessor
// and the type of it.
//
template <typename T>
inline
void load_accessor_into_memory_buffer(cgltf_accessor *accessor, T **memory_buffer_address, i64 *desired_count = NULL, cgltf_type *desired_type = NULL)
{
    assert(accessor->buffer_view && accessor->buffer_view->buffer);

    auto gltf_buffer   = accessor->buffer_view->buffer;
    auto buffer_offset = accessor->offset + accessor->buffer_view->offset;

    *memory_buffer_address = reinterpret_cast<T*>(reinterpret_cast<u8*>(gltf_buffer->data) + buffer_offset);

    if (desired_count) *desired_count = accessor->count;    
    if (desired_type)  *desired_type  = accessor->type;
}

void gltf_process_triangle_list_info(cgltf_data *parsed_gltf_data, Triangle_Mesh *triangle_mesh)
{
    i32 vertices_previous_lists = 0;
    i32 indices_previous_lists  = 0;

    auto c_path = (char*)(temp_c_string(triangle_mesh->full_path));
    bool use_colors = triangle_mesh->colors.count != 0;
    auto total_triangle_list_info = triangle_mesh->triangle_list_info.count;

    for (i32 it_index = 0; it_index < total_triangle_list_info; ++it_index)
    {
        auto sub_mesh = &parsed_gltf_data->meshes[it_index];
        auto total_primitives = sub_mesh->primitives_count;

        auto triangle_list_info = &triangle_mesh->triangle_list_info[it_index];
        assert(triangle_list_info->num_indices == 0); // Sanity check.
        assert(triangle_list_info->first_index == 0);

        for (i32 primitive_index = 0; primitive_index < total_primitives; ++primitive_index)
        {
            auto primitive = &sub_mesh->primitives[primitive_index];

            //
            // These are the memory buffers to the vertices data for this triangle list.
            // The *_counts are really just for validating the data of the model.
            //
            f32 *positions_memory = NULL;

            f32 *normals_memory = NULL;
            i64  normals_count;

            f32 *tangents_memory = NULL;
            i64  tangents_count;

            f32 *colors_memory = NULL;
            i64  colors_count;

            f32 *texture_uvs_memory = NULL;
            i64  texture_uvs_count;

            //
            // The invariant for these is that they must have the same count as the number of
            // vertices for each triangle list. Given this, any vertex in Blender/Maya that
            // has more than one UVs will be exported as multiple vertices in the same place.
            // I don't know how I should feel about this, but the way GLTF format works is
            // kind of like how GPU handle vertices so I can live with that.
            //
            u32 *bone_influences_memory = NULL;
            i64 bone_influences_count = 0;
            i64 num_bone_influences_per_vertex = 0;
            f32 *vertex_weights_memory  = NULL;
            i64 weights_count = 0;

            i64 vertices_this_list = 0;
            for (i32 attrib_index = 0; attrib_index < primitive->attributes_count; ++attrib_index)
            {
                auto attrib   = &primitive->attributes[attrib_index];
                auto accessor = attrib->data;

                switch (attrib->type)
                {
                    // Reading the vertices for this sub-mesh.
                    case cgltf_attribute_type_position: {
                        if (!positions_memory && (accessor->type == cgltf_type_vec3))
                        {
                            assert(accessor->component_type == cgltf_component_type_r_32f);
                            load_accessor_into_memory_buffer(accessor, &positions_memory, &vertices_this_list);
                        }
                    } break;
                    case cgltf_attribute_type_normal: {
                        if (!normals_memory && (accessor->type == cgltf_type_vec3))
                        {
                            assert(accessor->component_type == cgltf_component_type_r_32f);
                            load_accessor_into_memory_buffer(accessor, &normals_memory, &normals_count);
                        }
                    } break;
                    case cgltf_attribute_type_tangent: {
                        if (!tangents_memory && (accessor->type == cgltf_type_vec4))
                        {
                            assert(accessor->component_type == cgltf_component_type_r_32f);
                            load_accessor_into_memory_buffer(accessor, &tangents_memory, &tangents_count);
                        }
                    } break;
                    case cgltf_attribute_type_color: {
                        if (!colors_memory && (accessor->type == cgltf_type_vec3))
                        {
                            assert(accessor->component_type == cgltf_component_type_r_32f);
                            load_accessor_into_memory_buffer(accessor, &colors_memory, &colors_count);
                        }
                    } break;
                    case cgltf_attribute_type_texcoord: {
                        if (!texture_uvs_memory && (accessor->type == cgltf_type_vec2))
                        {
                            assert(accessor->component_type == cgltf_component_type_r_32f);
                            load_accessor_into_memory_buffer(accessor, &texture_uvs_memory, &texture_uvs_count);
                        }
                    } break;
                    case cgltf_attribute_type_joints: {
                        if (!bone_influences_memory) load_accessor_into_memory_buffer(accessor, &bone_influences_memory, &bone_influences_count);
                        else break;

                        num_bone_influences_per_vertex = cgltf_num_components(accessor->type);

                        assert(num_bone_influences_per_vertex <= MAX_MATRICES_PER_VERTEX); // Should not exceed the maximum amount otherwise, we are in trouble.

                        auto component_type = accessor->component_type;
                        assert((component_type == cgltf_component_type_r_8) || (component_type == cgltf_component_type_r_8u));
                    } break;
                    case cgltf_attribute_type_weights: {
                        if (!vertex_weights_memory) load_accessor_into_memory_buffer(accessor, &vertex_weights_memory, &weights_count);
                        else break;

                        assert(accessor->component_type == cgltf_component_type_r_32f);
                    } break;
                }
            }

            //
            // Checking the validity of the data...
            //
            {
                assert(normals_count == vertices_this_list);

                if (tangents_memory)    assert(tangents_count == normals_count);
                if (texture_uvs_memory) assert(texture_uvs_count == vertices_this_list);
                if (colors_memory)      assert(colors_count == vertices_this_list);

                // The number of vertex blend infos, or [bones influences and weights] must be the same as
                // the total number of vertices.
                if (bone_influences_memory || vertex_weights_memory) // Although we are doing an OR, both must be present at once.
                {
                    assert((bone_influences_count == vertices_this_list) && (weights_count == vertices_this_list));
                }
            }

            //
            // Process the positions, uvs, normals, tangents, bitangents, and colors.
            //
            for (i64 i = 0; i < vertices_this_list; ++i)
            {
                auto v_i = vertices_previous_lists + i;

                // Positions.
                assert(positions_memory);

                auto p = &positions_memory[i * 3];
                triangle_mesh->vertices[v_i] = Vector3(p[0], p[1], p[2]);

                // Frame3
                assert(normals_memory);

                auto v_frame = &triangle_mesh->vertex_frames[v_i];
                auto n = &normals_memory[i * 3];
                v_frame->normal = Vector3(n[0], n[1], n[2]);

                if (tangents_memory)
                {
                    auto t = &tangents_memory[i * 4]; // The fourth component is the scalar of the first 3.
                    v_frame->tangent = Vector3(t[0], t[1], t[2]) * t[3];
                }

                // Texture UVs.
                if (texture_uvs_memory)
                {
                    auto uv = &texture_uvs_memory[i * 2];
                    triangle_mesh->uvs[v_i] = Vector2(uv[0], 1 - uv[1]); // Flipping the y coordinate here because GLTF stores the images in the right order and stbi image flipped them....
                }

                // Colors.
                if (use_colors)
                {
                    if (colors_memory)
                    {
                        auto c = &colors_memory[i * 3]; // 3 components because the last one is automatically 1.0f.
                        triangle_mesh->colors[v_i] = Vector4(c[0], c[1], c[2], 1.0f);
                    }
                    else
                    {
                        triangle_mesh->colors[v_i] = Vector4(1, 1, 1, 1);
                    }
                }

                // Bones and vertex weights
                if (bone_influences_memory && vertex_weights_memory)
                {
                    assert(triangle_mesh->skeleton_info->vertex_blend_info);

                    auto ids     = &reinterpret_cast<u8*>(bone_influences_memory)[i * num_bone_influences_per_vertex];
                    auto weights = &vertex_weights_memory[i * num_bone_influences_per_vertex];

                    auto blend_info   = &triangle_mesh->skeleton_info->vertex_blend_info[v_i];
                    auto num_matrices = 0;

                    for (i32 j = 0; j < MAX_MATRICES_PER_VERTEX; ++j)
                    {
                        if (!weights[j]) continue; // If weight is 0, then that matrix doesn't influence the vertex.

                        auto piece = &blend_info->pieces[num_matrices];

                        // @Note: This index is the joint/skeleton node index
                        // inside the joints array of the skin.
                        auto matrix_index  = static_cast<i32>(ids[j]);
                        assert(matrix_index < triangle_mesh->skeleton_info->skeleton_node_info.count);

                        piece->matrix_index  = matrix_index;
                        piece->matrix_weight = weights[j];

                        num_matrices += 1;
                    }

                    assert(num_matrices <= num_bone_influences_per_vertex);
                    blend_info->num_matrices = num_matrices;
                }
            }

            if (!tangents_memory)
            {
                // @Incomplete: Supposed to calculate the tangents and bitangents based on the normals and vertices...
                logprint("mesh_catalog", "We need to calculate tangents and bitangents for the model '%s'!\n", c_path);
            }

            // @Incomplete: Not handling texture color... or should we??

            //
            // Process the indices for this sub-mesh.
            //
            {
                auto indices_accessor  = primitive->indices;
                auto indices_this_list = indices_accessor->count;
                auto indices_offset    = indices_previous_lists;

                assert(!indices_accessor->is_sparse);
                assert(indices_accessor->buffer_view);

                triangle_list_info->num_indices += indices_this_list;

                // Only setting the first index of the triangle list if this is the first primitive.
                if (primitive_index == 0)
                {
                    triangle_list_info->first_index = indices_offset;
                }

                u8 *dest = reinterpret_cast<u8*>(&triangle_mesh->index_array[indices_offset]);

                for (i64 i = 0; i < indices_this_list; ++i)
                {
                    auto vertex_index = static_cast<u32>(cgltf_accessor_read_index(indices_accessor, i));
                    ((u32*)dest)[i] = vertices_previous_lists + vertex_index;
                }

                indices_previous_lists += indices_this_list;
            }

            vertices_previous_lists += vertices_this_list;

            //
            // Assign the material to the triangle list, if this is the first primitive.
            // This is because we only allow for one material per triangle list, and
            // I assume that most of the materials are uploaded in the first primitive.
            // @Incomplete: Comment about the weird case with sponza.glb.
            //
            if (primitive_index == 0)
            {
                auto material_this_list = primitive->material;
                if (material_this_list)
                {
                    auto material_index = cgltf_material_index(parsed_gltf_data, material_this_list);
                    auto material_info  = &triangle_mesh->material_info[material_index]; // Where we store the materials.

                    triangle_list_info->material_index = material_index;
                    // triangle_list_info->map = material_info->albedo_2024;
                }
                else
                {
                    logprint("gltf_loader", "Error: Sub-mesh '%s' has no materials,....\n", sub_mesh->name);
                    // @Incomplete: Should be using the white texture or something here.
                    assert(0);
                }
            }

            // @Incomplete: Do bounding box!!!
            // @Incomplete: Do bounding box!!!
            // @Incomplete: Do bounding box!!!
        }
    }
}

// Mostly @Cutnpaste from the gltf_process_triangle_list_info().
// I did this so that I don't have to litter the other function with a bunch of ifs.
// This is in case we want to extend this into supporting multiple primitives per triangle list.
// Until then, we will use this.
void gltf_process_single_mesh_multiple_primitives_triangle_list(cgltf_data *parsed_gltf_data, Triangle_Mesh *triangle_mesh)
{
    i32 vertices_previous_lists = 0;
    i32 indices_previous_lists  = 0;

    auto c_path = (char*)(temp_c_string(triangle_mesh->full_path));
    bool use_colors = triangle_mesh->colors.count != 0;
    auto total_triangle_list_info = triangle_mesh->triangle_list_info.count;

    auto first_mesh = &parsed_gltf_data->meshes[0];
    for (i32 it_index = 0; it_index < total_triangle_list_info; ++it_index)
    {
        // In this case, primitive and triangle list are considered to be the same thing (@Note that
        // this is not true always, only because this function deals with a degenerate case).
        auto primitive = &first_mesh->primitives[it_index];

        auto triangle_list_info = &triangle_mesh->triangle_list_info[it_index];
        assert(triangle_list_info->num_indices == 0); // Sanity check.
        assert(triangle_list_info->first_index == 0);

        //
        // These are the memory buffers to the vertices data for this triangle list.
        // The *_counts are really just for validating the data of the model.
        //
        f32 *positions_memory = NULL;

        f32 *normals_memory = NULL;
        i64  normals_count;

        f32 *tangents_memory = NULL;
        i64  tangents_count;

        f32 *colors_memory = NULL;
        i64  colors_count;

        f32 *texture_uvs_memory = NULL;
        i64  texture_uvs_count;

        //
        // The invariant for these is that they must have the same count as the number of
        // vertices for each triangle list. Given this, any vertex in Blender/Maya that
        // has more than one UVs will be exported as multiple vertices in the same place.
        // I don't know how I should feel about this, but the way GLTF format works is
        // kind of like how GPU handle vertices so I can live with that.
        //
        u32 *bone_influences_memory = NULL;
        i64 bone_influences_count = 0;
        i64 num_bone_influences_per_vertex = 0;
        f32 *vertex_weights_memory  = NULL;
        i64 weights_count = 0;

        i64 vertices_this_list = 0;
        for (i32 attrib_index = 0; attrib_index < primitive->attributes_count; ++attrib_index)
        {
            auto attrib   = &primitive->attributes[attrib_index];
            auto accessor = attrib->data;

            switch (attrib->type)
            {
                // Reading the vertices for this sub-mesh.
                case cgltf_attribute_type_position: {
                    if (!positions_memory && (accessor->type == cgltf_type_vec3))
                    {
                        assert(accessor->component_type == cgltf_component_type_r_32f);
                        load_accessor_into_memory_buffer(accessor, &positions_memory, &vertices_this_list);
                    }
                } break;
                case cgltf_attribute_type_normal: {
                    if (!normals_memory && (accessor->type == cgltf_type_vec3))
                    {
                        assert(accessor->component_type == cgltf_component_type_r_32f);
                        load_accessor_into_memory_buffer(accessor, &normals_memory, &normals_count);
                    }
                } break;
                case cgltf_attribute_type_tangent: {
                    if (!tangents_memory && (accessor->type == cgltf_type_vec4))
                    {
                        assert(accessor->component_type == cgltf_component_type_r_32f);
                        load_accessor_into_memory_buffer(accessor, &tangents_memory, &tangents_count);
                    }
                } break;
                case cgltf_attribute_type_color: {
                    if (!colors_memory && (accessor->type == cgltf_type_vec3))
                    {
                        assert(accessor->component_type == cgltf_component_type_r_32f);
                        load_accessor_into_memory_buffer(accessor, &colors_memory, &colors_count);
                    }
                } break;
                case cgltf_attribute_type_texcoord: {
                    if (!texture_uvs_memory && (accessor->type == cgltf_type_vec2))
                    {
                        assert(accessor->component_type == cgltf_component_type_r_32f);
                        load_accessor_into_memory_buffer(accessor, &texture_uvs_memory, &texture_uvs_count);
                    }
                } break;
                case cgltf_attribute_type_joints: {
                    if (!bone_influences_memory) load_accessor_into_memory_buffer(accessor, &bone_influences_memory, &bone_influences_count);
                    else break;

                    num_bone_influences_per_vertex = cgltf_num_components(accessor->type);

                    assert(num_bone_influences_per_vertex <= MAX_MATRICES_PER_VERTEX); // Should not exceed the maximum amount otherwise, we are in trouble.

                    auto component_type = accessor->component_type;
                    assert((component_type == cgltf_component_type_r_8) || (component_type == cgltf_component_type_r_8u));
                } break;
                case cgltf_attribute_type_weights: {
                    if (!vertex_weights_memory) load_accessor_into_memory_buffer(accessor, &vertex_weights_memory, &weights_count);
                    else break;

                    assert(accessor->component_type == cgltf_component_type_r_32f);
                } break;
            }
        }

        //
        // Checking the validity of the data...
        //
        {
            assert(normals_count == vertices_this_list);

            if (tangents_memory)    assert(tangents_count == normals_count);
            if (texture_uvs_memory) assert(texture_uvs_count == vertices_this_list);
            if (colors_memory)      assert(colors_count == vertices_this_list);

            // The number of vertex blend infos, or [bones influences and weights] must be the same as
            // the total number of vertices.
            if (bone_influences_memory || vertex_weights_memory) // Although we are doing an OR, both must be present at once.
            {
                assert((bone_influences_count == vertices_this_list) && (weights_count == vertices_this_list));
            }
        }

        //
        // Process the positions, uvs, normals, tangents, bitangents, and colors.
        //
        for (i64 i = 0; i < vertices_this_list; ++i)
        {
            auto v_i = vertices_previous_lists + i;
            
            // Positions.
            assert(positions_memory);
            auto p = &positions_memory[i * 3];
            triangle_mesh->vertices[v_i] = Vector3(p[0], p[1], p[2]);

            // Frame3
            assert(normals_memory);
            auto v_frame = &triangle_mesh->vertex_frames[v_i];
            auto n = &normals_memory[i * 3];
            v_frame->normal = Vector3(n[0], n[1], n[2]);

            if (tangents_memory)
            {
                auto t = &tangents_memory[i * 4]; // The fourth component is the scalar of the first 3.
                v_frame->tangent = Vector3(t[0], t[1], t[2]) * t[3];
            }

            // Texture UVs.
            if (texture_uvs_memory)
            {
                auto uv = &texture_uvs_memory[i * 2];
                triangle_mesh->uvs[v_i] = Vector2(uv[0], 1 - uv[1]); // Flipping the y coordinate here because GLTF stores the images in the right order and stbi image flipped them....
            }

            // Colors.
            if (use_colors)
            {
                if (colors_memory)
                {
                    auto c = &colors_memory[i * 3]; // 3 components because the last one is automatically 1.0f.
                    triangle_mesh->colors[v_i] = Vector4(c[0], c[1], c[2], 1.0f);
                }
                else
                {
                    triangle_mesh->colors[v_i] = Vector4(1, 1, 1, 1);
                }
            }

            // Bones and vertex weights
            if (bone_influences_memory && vertex_weights_memory)
            {
                assert(triangle_mesh->skeleton_info->vertex_blend_info);

                auto ids     = &reinterpret_cast<u8*>(bone_influences_memory)[i * num_bone_influences_per_vertex];
                auto weights = &vertex_weights_memory[i * num_bone_influences_per_vertex];

                auto blend_info   = &triangle_mesh->skeleton_info->vertex_blend_info[v_i];
                auto num_matrices = 0;

                for (i32 j = 0; j < MAX_MATRICES_PER_VERTEX; ++j)
                {
                    if (!weights[j]) continue; // If weight is 0, then that matrix doesn't influence the vertex.

                    auto piece = &blend_info->pieces[num_matrices];

                    // @Note: This index is the joint/skeleton node index
                    // inside the joints array of the skin.
                    auto matrix_index  = static_cast<i32>(ids[j]);
                    assert(matrix_index < triangle_mesh->skeleton_info->skeleton_node_info.count);

                    piece->matrix_index  = matrix_index;
                    piece->matrix_weight = weights[j];

                    num_matrices += 1;
                }

                assert(num_matrices <= num_bone_influences_per_vertex);
                blend_info->num_matrices = num_matrices;
            }
        }

        if (!tangents_memory)
        {
            // @Incomplete: Supposed to calculate the tangents and bitangents based on the normals and vertices...
            logprint("mesh_catalog", "We need to calculate tangents and bitangents for the model '%s'!\n", c_path);
        }

        // @Incomplete: Not handling texture color... or should we??

        //
        // Process the indices for this triangle list.
        //
        {
            auto indices_accessor  = primitive->indices;
            auto indices_this_list = indices_accessor->count;
            auto indices_offset    = indices_previous_lists;

            assert(!indices_accessor->is_sparse);
            assert(indices_accessor->buffer_view);

            triangle_list_info->num_indices += indices_this_list;

            triangle_list_info->first_index = indices_offset; // This differs with the other function.

            u8 *dest = reinterpret_cast<u8*>(&triangle_mesh->index_array[indices_offset]);

            for (i64 i = 0; i < indices_this_list; ++i)
            {
                auto vertex_index = static_cast<u32>(cgltf_accessor_read_index(indices_accessor, i));
                ((u32*)dest)[i] = vertices_previous_lists + vertex_index;
            }

            indices_previous_lists += indices_this_list;
        }

        vertices_previous_lists += vertices_this_list;

        //
        // The only thing that is different right now between the two functions is this
        // part where we process the materials. With the regular process_triangle_list,
        // we only do the material for the first primitive, whereas this one, we
        // consider each primitive an individual triangle list, so we process the materials.
        // of each list differently.
        //
        auto material_this_list = primitive->material;
        if (material_this_list)
        {
            auto material_index = cgltf_material_index(parsed_gltf_data, material_this_list);
            auto material_info  = &triangle_mesh->material_info[material_index]; // Where we store the materials.a

            // printf("Using material at index %ld\n", material_index);

            triangle_list_info->material_index = material_index;
            // triangle_list_info->map = material_info->albedo_2024;
        }
        else
        {
            logprint("gltf_loader", "Error: Sub-mesh %d of model '%s' has no materials,....\n", it_index, c_path);
            // @Incomplete: Should be using the white texture or something here.
            assert(0);
        }

        // @Incomplete: Do bounding box!!!
        // @Incomplete: Do bounding box!!!
        // @Incomplete: Do bounding box!!!
    }
}

bool load_gltf_model_2024(Triangle_Mesh *triangle_mesh) // @Cleanup: Rename
{
    assert((triangle_mesh->full_path));
    auto model_path_relative_to_exe = triangle_mesh->full_path;
    auto c_path = (char*)(temp_c_string(model_path_relative_to_exe));

    cgltf_options loader_options = {};
    cgltf_data *parsed_gltf_data = NULL;

    auto success = cgltf_parse_file(&loader_options, c_path, &parsed_gltf_data);
    if (success != cgltf_result_success)
    {
        logprint("gltf_loader", "Not able to parsed model from file '%s'!\n", c_path);
        return false;
    }

    defer { cgltf_free(parsed_gltf_data); };
    assert((parsed_gltf_data->file_type == cgltf_file_type_gltf) || (parsed_gltf_data->file_type == cgltf_file_type_glb));

    success = cgltf_load_buffers(&loader_options, parsed_gltf_data, c_path);
    if (success != cgltf_result_success)
    {
        logprint("gltf_loader", "Not able to parsed the .bin file from model '%s'!\n", c_path);
        return false;
    }

    if (!parsed_gltf_data->meshes_count)
    {
        logprint("gltf_loader", "No meshes found in file '%s'!\n", c_path);
        return false;
    }

    i32 total_vertices = 0;
    i32 total_faces    = 0;
    // Triangle list also means the sub-meshes in the model file.
    // A model may have a different material for his clothing or
    // his footwear, so we usually split them out to different
    // sub-meshes. Here, I use the term triangle list info for
    // the sub-meshes.
    i32 total_triangle_list_info = parsed_gltf_data->meshes_count;

    if (!(total_triangle_list_info))
    {
        logprint("gltf_loader", "ERROR: Model '%s' contains no mesh components/parts.\n", c_path);
        return false;
    }

    // These are true if at least one of the sub-meshes uses them.
    // @Cleanup: Currently, we allocate the colors to the
    // amount of the vertices, but this is not so great, because
    // not every triangle list uses colors, so
    // we should consider allocating the right amount and keep
    // the first index of colors inside the triangle list
    // info themselves.
    bool use_colors = false;

    // @Incomplete: Comment me.
    bool is_single_mesh_multiple_primitives = false;
    i64  all_primitives_count; // This is only meaningful if single_mesh_multiple_primitives == true.

    //
    // Pass 1: Get total number of vertices.
    //
    for (i32 it_index = 0; it_index < total_triangle_list_info; ++it_index)
    {
        auto sub_mesh         = &parsed_gltf_data->meshes[it_index];
        auto total_primitives = sub_mesh->primitives_count;

        if ((total_triangle_list_info == 1) && (total_primitives > 1))
        {
            is_single_mesh_multiple_primitives = true;
            all_primitives_count = total_primitives;
        }

        for (i32 primitive_index = 0; primitive_index < total_primitives; ++primitive_index)
        {
            auto primitive = &sub_mesh->primitives[primitive_index];

            // We add up the total amount of indices to the faces, because,
            // we know that we triangulate everything up front, so after 
            // looping through all the sub-meshes, we can just divide
            // the total indices by 3 to get the number of faces.
            total_faces += primitive->indices->count;

            cgltf_accessor *colors_accessor = NULL;

            for (i32 attrib_index = 0; attrib_index < primitive->attributes_count; ++attrib_index)
            {
                auto attrib   = &primitive->attributes[attrib_index];
                auto accessor = attrib->data;

                switch (attrib->type)
                {
                    // Reading the vertices for this sub-mesh.
                    case cgltf_attribute_type_position: {
                        auto vertices_this_sub_mesh = accessor->count;
                        total_vertices += vertices_this_sub_mesh;
                    } break;
                    case cgltf_attribute_type_color: {
                        if (accessor->type == cgltf_type_vec3) colors_accessor = accessor;
                    } break;
                }
            }

            use_colors = use_colors && (colors_accessor != NULL);
        }
    }

    total_faces /= 3; // We divide the total faces by 3 because total_faces = total_indices / 3. Assuming that the mesh is triangulated.

    // printf("Vertices count for model '%s' is %d\n", c_path, total_vertices);
    // printf("Faces count for model '%s' is %d\n", c_path, total_faces);

    //
    // Allocating space for the vertices, indices, triangle list infos, and materials.
    //
    allocate_geometry(triangle_mesh, total_vertices, total_faces);

    if (is_single_mesh_multiple_primitives)
    {
        // Allocate the triangle list info differently because this is a degenerate type.
        allocate_triangle_list_info(triangle_mesh, all_primitives_count);
    }
    else
    {
        allocate_triangle_list_info(triangle_mesh, total_triangle_list_info);
    }

    if (!(total_vertices && total_faces))
    {
        logprint("gltf_loader", "Error: Model '%s' contains no vertices or faces.\n", c_path);
        return false;
    }

    if (use_colors)   allocate_color_data(triangle_mesh);
    // if (use_textures) allocate_texture_colors(triangle_mesh);

    auto materials_count = parsed_gltf_data->materials_count;
    if (materials_count) allocate_materials(triangle_mesh, materials_count);

    //
    // Pass 2: Load the textures.
    //
    {
        // In the GLTF format, images are what our engine called textures.
        // They can contains path to the textures or embedded data for the textures.
        // This can been seen via the 'uri' properties of the model.
        // There is also the term 'texture', and 'sampler' in GLTF.
        // Essentially, 'texture' type stores the image's index and
        // the sampler's index, while samplers store the attributes of how
        // an image should be processed (via the 'magFilter', 'minFilter', 'wrapS', and 'wrapT'
        // fields.)

        // Just so that we don't modify the heck out of the model path.
        auto relative_model_path = model_path_relative_to_exe;
        // Stripping away the file name to get the model path leading up to it.
        auto index_of_last_slash = find_index_from_right(relative_model_path, '/');
        relative_model_path.count -= relative_model_path.count - index_of_last_slash;

        auto gltf_images_count = parsed_gltf_data->images_count;

        // Allocating space for the texture lookup.
        array_reserve(&textures_lookup, gltf_images_count);
        textures_lookup.count = gltf_images_count;

        for (i64 gltf_image_index = 0; gltf_image_index < gltf_images_count; ++gltf_image_index)
        {
            auto gltf_image   = &parsed_gltf_data->images[gltf_image_index];
            auto texture_name = String(gltf_image->name); // Using the term texture_name because we are using this variable to find stuff in the texture catalog.
            auto texture_map  = catalog_find(&texture_catalog, texture_name, false);

            //
            // :sRGB
            // @Fixme: @Hack: We are straight up making every texture we loaded a sRGB thing. This is wrong
            // because not every texture in the model is sRGB (although most of them are).
            // It is also wrong for someone who wants to use these textures later after we resolve to LDR.
            //
            if (texture_map)
            {
                // printf("Texture map '%s' of path '%s' was already loaded!\n", temp_c_string(texture_map->name), temp_c_string(texture_map->full_path));

                if (!texture_map->is_srgb) // Only load it again if it is not sRGB already. @Speed: This could be sped up because we should not just load the thing and then load it again...
                {
                    load_texture_from_file(texture_map, true); // @Speed: We are loading the image again for the sRGB...
                    textures_lookup[gltf_image_index] = texture_map;
                }
                
                continue;
            }

            if (gltf_image->uri)
            {
                printf("Try to load image from a uri but it wasn't found in the catalog?\n");
                assert(0); // I think this can happen with glTF but since we have the catalog system already, things should not go here.

                // @Important: We should always remember to export the models with textures relative to its location.
                auto texture_path_relative_to_exe = tprint(String("%s/%s"), temp_c_string(relative_model_path), gltf_image->uri);

                my_register_loose_file<Texture_Map>(&texture_catalog.base, texture_name, texture_path_relative_to_exe);
                textures_lookup[gltf_image_index] = catalog_find(&texture_catalog, texture_name); // We are doing it this way to force it to reload.
            }
            else
            {
                printf("Loading texture '%s' from memory '%s'!\n", texture_name.data, c_path);

                auto image_buffer_view = gltf_image->buffer_view;
                if (image_buffer_view)
                {
                    auto image_buffer = image_buffer_view->buffer; 
                    i64 size_to_read = image_buffer_view->size;
                    u8 *src = reinterpret_cast<u8*>(image_buffer->data) + image_buffer_view->offset;

                    // Using the path relative to exe for the texture's full path because it is embedded.
                    auto texture_map = make_placeholder(&texture_catalog, texture_name, model_path_relative_to_exe); 

                    auto success = load_texture_from_memory(texture_map, src, size_to_read, true);
                    if (!success)
                    {
                        logprint("gltf_loader", "Failed to load embedded image '%s' of model '%s' from memory!\n", texture_name.data, c_path);
                        return false;
                    }

                    texture_map->loaded = true; // Set it to loaded in case we load the model twice so it wouldn't need to reload the asset.

                    // @Cleanup: We are manually adding the texture back to the catalog...
                    String table_key = copy_string(texture_name);
                    table_add(&texture_catalog.table, table_key, texture_map);

                    textures_lookup[gltf_image_index] = texture_map;

                    // Get minification and magnification filters.
                    // There might be duplicated textures of the same image and samplers,
                    // so what we do is to loop from the image index and start probing from there.
                    auto corresponding_sampler = get_sampler_of_image(parsed_gltf_data, gltf_image_index, gltf_image);

                    // These values corresponds to the GL_* flag to set for GL_TEXTURE_MIN_FILTER, *_MAG_FILTER, *_WRAP_S, *_WRAP_T.
                    auto min_filter = corresponding_sampler->min_filter; // @Incomplete: Attribute is not used in set_texture()
                    auto mag_filter = corresponding_sampler->mag_filter; // @Incomplete: Attribute is not used in set_texture()
                    auto wrap_s     = corresponding_sampler->wrap_s; // @Incomplete: Attribute is not used in set_texture()
                    auto wrap_t     = corresponding_sampler->wrap_t; // @Incomplete: Attribute is not used in set_texture()
                }
                else
                {
                    logprint("gltf_loader", "What should i do to %s?? It's not an embedded texture, nor is it a separate file!\n", texture_name.data);
                    assert(0);
                }
            }
        }
    }

    //
    // Pass 3: Load skinning info.
    //
    {
        auto skeletons_count = parsed_gltf_data->skins_count;
        if (skeletons_count)
        {
            if (skeletons_count > 1)
            {
                logprint("gltf_loader", "The model '%s' should only contains one skeleton, but we found %ld upon loading! Using the first one...\n", c_path, skeletons_count);
            }

            //
            // Allocate the space for the mapping from vertex index to the vertex blend info
            // up-front.
            // The vertex index directly corresponds to the vertex_blend_info array, isn't it?
            //
            auto vertices_count = triangle_mesh->vertices.count;
            assert(vertices_count); // Must have the count of the vertices first.

            // triangle_mesh->vertex_to_vertex_blend_info_map = NewArray<i32>(vertices_count);

            auto skeleton_data = &parsed_gltf_data->skins[0]; // Using the first skeleton, always.
            auto inverse_bind_matrices_accessor = skeleton_data->inverse_bind_matrices;

            if (inverse_bind_matrices_accessor)
            {
                // Only here we allocate the space for the skeleton info, because if the skeleton
                // does not contain some inverse bind matrices, then I don't know what to do...
                auto info = New<Skeleton_Info>();

                //
                // This seems like a big issue, so we have to think about how to map
                // node or joint from its index.
                //
                auto skeleton_nodes_count = skeleton_data->joints_count;

                info->skeleton_node_info = NewArray<Skeleton_Node_Info>(skeleton_nodes_count);
                info->vertex_blend_info  = NewArray<Vertex_Blend_Info>(vertices_count);

                // Number of matrices that transform the node from
                // the rest object space (the bind pose), to the
                // object space itself.
                i64 matrices_count = -1;
                Matrix4 *matrices_memory = NULL;

                assert(skeleton_data->inverse_bind_matrices->type == cgltf_type_mat4);
                load_accessor_into_memory_buffer(skeleton_data->inverse_bind_matrices, &matrices_memory, &matrices_count);

                // Since every node supposed to have an inverse bind matrice,
                // the number of matrices must be the same as the number of
                // nodes.
                assert(matrices_count == skeleton_nodes_count);

                for (i64 it_index = 0; it_index < skeleton_nodes_count; ++it_index)
                {
                    auto gltf_node = skeleton_data->joints[it_index];

                    // auto node_index_globally    = cgltf_node_index(parsed_gltf_data, gltf_node);
                    // auto node_index_in_skeleton = flip_gltf_skeleton_node_index(info, node_index_globally);

                    // @Investigate: I don't know if this is right or not.
                    auto node = &info->skeleton_node_info[it_index]; // node_index_in_skeleton
                    node->name = copy_string(String(gltf_node->name));

                    // printf("Mesh catalog, mapping of '%s' with index %ld\n", gltf_node->name, it_index);

                    Matrix4 inverse_m = matrices_memory[it_index];
                    node->rest_object_space_to_object_space = inverse_m; // @Investigate:
                }

                triangle_mesh->skeleton_info = info;
            }
        }
    }

    //
    // Pass 4: Load the materials.
    //
    {
        for (i64 material_index = 0; material_index < materials_count; ++material_index)
        {
            // These two arrays have the same sizes, because we allocate the exact
            // amount as the materials in the gltf parsed data.
            auto material      = &triangle_mesh->material_info[material_index];
            auto gltf_material = &parsed_gltf_data->materials[material_index];

            //
            // @Investigate: In PBR rendering, there are 2 type of structuring the material:
            // - The first way is to use the ``metallic & roughness'' idea.
            // - The second way is to use the ``specular glossiness'' idea.
            // @Incomplete: Comment about this after @Investigate.
            //

            if (gltf_material->has_pbr_metallic_roughness)
            {
                auto pbr_metallic_roughness = &gltf_material->pbr_metallic_roughness;

                // The base color. If there is a diffuse or albedo texture associated with
                // this material, that texture will be scaled by this base color factor.
                // If there isn't a texture associated with this 
                auto c = pbr_metallic_roughness->base_color_factor;
                auto base_color_factor = Vector4(c[0], c[1], c[2], c[3]);

                material->color = base_color_factor;

                // Constants for roughness, and metallicness. // @Incomplete: Not using these values.
                auto roughness_factor = pbr_metallic_roughness->roughness_factor;
                auto metallic_factor  = pbr_metallic_roughness->metallic_factor;
                // printf("In material '%s'!\n", gltf_material->name);
                // printf("Roughness factor %f\n", roughness_factor);
                // printf("Metallic  factor %f\n", metallic_factor);

                auto albedo_texture = &pbr_metallic_roughness->base_color_texture;
                if (!albedo_texture->texture)
                {
                    logprint("Material system", "Material '%s' for model '%s' does not contains a texture image, using the white texture with the base color!\n", gltf_material->name, c_path);

                    material->albedo_2024 = white_texture;
                }
                else
                {
                    auto gltf_albedo_index = cgltf_image_index(parsed_gltf_data, albedo_texture->texture->image);
                    auto gltf_albedo_name  = albedo_texture->texture->image->name;
                    auto albedo_map = textures_lookup[gltf_albedo_index];

                    assert(albedo_map);
                    material->albedo_2024 = albedo_map;
                }

                // Texture for roughness and metallicness
                auto metallic_roughness_texture = &pbr_metallic_roughness->metallic_roughness_texture;
                if (metallic_roughness_texture->texture)
                {
                    // logprint("Met-Rough", "In material '%s':\n", gltf_material->name);
                    // logprint("Met-Rough", "   - texcoord = %d\n", metallic_roughness_texture->texcoord);
                    // logprint("Met-Rough", "   - scale = %f\n", metallic_roughness_texture->scale);
                    // logprint("Met-Rough", "   - has_transform = %d\n", metallic_roughness_texture->has_transform);

                    auto image = metallic_roughness_texture->texture->image;
                    if (image)
                    {
                        auto metallic_roughness_index = cgltf_image_index(parsed_gltf_data, image);
                        auto metallic_roughness_map = textures_lookup[metallic_roughness_index];

                        // logprint("Met-Rough", "   - Image name is '%s'!\n", image->name);
                        // printf("The image index is %ld\n", metallic_roughness_index);
                    }

                    // newline();
                    // @Incomplete: Figure out what this does.
                    // assert(0);
                }
            }

            // The normal map.
            auto normal_texture = &gltf_material->normal_texture;
            if (normal_texture->texture)
            {
                auto gltf_normal_index = cgltf_image_index(parsed_gltf_data, normal_texture->texture->image);
                auto gltf_normal_name  = normal_texture->texture->image->name;
                auto normal_map = textures_lookup[gltf_normal_index];

                assert(normal_map);

                auto normal_map_intensity = normal_texture->scale;

                // logprint("Normal map", "Normal map '%s' for material '%s'\n", gltf_normal_name, gltf_material->name);
                // logprint("Normal map", "Normal map intensity is %f\n", normal_map_intensity);
                // newline();

                // @Incomplete: How to use normal maps?

                material->normal_map = normal_map;
                material->normal_map_intensity = normal_map_intensity;
            }

            // The ambient occlusion map.
            auto ao_texture = &gltf_material->occlusion_texture;
            if (ao_texture->texture)
            {
                auto gltf_ao_name = ao_texture->texture->image->name;
                logprint("Ambient Occlusion", "Material '%s' has ambient occlusion texture '%s'!\n", gltf_material->name, gltf_ao_name);

                assert(0);
            }

            // The emissive texture (sometimes, this will use the AO map)
            auto emissive_texture = &gltf_material->emissive_texture;
            if (emissive_texture->texture)
            {
                auto gltf_emissive_name = emissive_texture->texture->image->name;
                logprint("Emissive", "Material '%s' has emissive texture '%s'\n", gltf_material->name, gltf_emissive_name);
            }

            if (gltf_material->has_pbr_specular_glossiness)
            {
                logprint("Material stuff", "Material '%s' has uses the pbr specular glossiness workflow, not supported yet!\n", gltf_material->name);

                assert(0); // @Incomplete:
            }

            if (gltf_material->has_specular)
            {
                printf("Material '%s' has pbr specular!\n", gltf_material->name);

                auto gltf_specular          = &gltf_material->specular;
                auto specular_texture       = &gltf_specular->specular_texture;
                auto specular_color_texture = &gltf_specular->specular_color_texture;

                if (specular_texture->texture)
                {
                    logprint("Material stuff", "Specular image is '%s'!\n", specular_texture->texture->image->name);
                    assert(0); // @Incomplete:
                }

                if (specular_color_texture->texture)
                {
                    logprint("Material stuff", "Specular color image is '%s'!\n", specular_color_texture->texture->image->name);
                    assert(0); // @Incomplete:
                }

                // auto specular_factor = gltf_specular->specular_factor;
                // auto specular_color_factor = gltf_specular->specular_color_factor;
                // @Incomplete:
            }

            if (gltf_material->has_ior)
            {
                printf("Material '%s' has ior!\n", gltf_material->name);
                assert(0);
            }
        }
    }

    //
    // Pass 5: Process the triangle list infos
    //
    if (!is_single_mesh_multiple_primitives)
    {
        gltf_process_triangle_list_info(parsed_gltf_data, triangle_mesh);
    }
    else
    {
        // Degenerate case of one mesh but multiple primitives inside it (Example: sponza).
        gltf_process_single_mesh_multiple_primitives_triangle_list(parsed_gltf_data, triangle_mesh);
    }

    return true;
}

// #include <ufbx.h>
// bool loads_fbx_model(Triangle_Mesh *triangle_mesh)
// {
//     return true;

//     assert((triangle_mesh->full_path));
//     auto full_path   = triangle_mesh->full_path;
//     auto c_path      = (char*)(temp_c_string(full_path));
//     auto extension   = find_character_from_right(full_path, '.');
//     advance(&extension, 1); // Skip the '.'

//     assert(extension == String("fbx"));

//     ufbx_error fbx_error;
//     ufbx_load_opts load_options = {};
//     load_options.target_axes        = ufbx_axes_right_handed_z_up;
//     load_options.target_unit_meters = 1.0f;
//     load_options.space_conversion   = UFBX_SPACE_CONVERSION_MODIFY_GEOMETRY;

//     auto scene_data = ufbx_load_file(c_path, &load_options, &fbx_error);
//     if (!scene_data)
//     {
//         logprint("load_fbx_model", "Failed to load the fbx file '%s' due to error: %s!\n", c_path, fbx_error.description.data);
//         return false;
//     }

//     defer { ufbx_free_scene(scene_data); };

//     Bounding_Box bounding_box;
//     init_bounding_box(&bounding_box);

//     i32 total_vertices = 0;
//     i32 total_faces    = 0;
//     i32 test_total_indices = 0;

//     i32 total_triangle_list_info = scene_data->meshes.count;
//     printf("total triangle list is %d\n", total_triangle_list_info);

//     //
//     // Pass 1: Get total number of vertices, faces by traversing through all the meshes.
//     //    
//     for (i32 i = 0; i < total_triangle_list_info; ++i)
//     {
//         auto mesh = scene_data->meshes[i];

//         total_vertices += mesh->num_vertices;
//         total_faces    += mesh->num_faces;
//         test_total_indices += mesh->num_indices;

//         printf("Num triangles is %ld\n", mesh->num_triangles);
//         printf("Max face triangles is %ld\n", mesh->max_face_triangles);

//         printf("skin deformers count %ld\n", mesh->skin_deformers.count);
//         constexpr i32 MAX_BONES = 64;
//         i32 bones_count = 0;
//         if (mesh->skin_deformers.count)
//         {
//             auto skin = mesh->skin_deformers[0];

//             for (i32 cluster_index = 0; cluster_index < skin->clusters.count; ++cluster_index)
//             {
//                 auto cluster = skin->clusters[cluster_index];

//                 if (bones_count >= MAX_BONES) break;

//                 auto bone_node  = cluster->bone_node;
//                 auto bone_index = bone_node->typed_id;

//                 auto m = cluster->geometry_to_bone;
//                 Matrix4 bone_m = Matrix4(1.0f); // Binding matrix from local mesh vertices to the bone.
//                 bone_m[0][0] = m.m00;  bone_m[1][0] = m.m01;  bone_m[2][0] = m.m02;  bone_m[3][0] = m.m03;
//                 bone_m[0][1] = m.m10;  bone_m[1][1] = m.m11;  bone_m[2][1] = m.m12;  bone_m[3][1] = m.m13;
//                 bone_m[0][2] = m.m20;  bone_m[1][2] = m.m21;  bone_m[2][2] = m.m22;  bone_m[3][2] = m.m23;

//                 print_cmaj_as_rmaj(bone_m);

//                 bones_count += 1;
//             }
//         }
//     }

//     printf("total faces is %d\n", total_faces);
//     printf("total vertices %d\n", total_vertices);
//     printf("total indices %d\n", test_total_indices);

//     // assert(test_total_indices == (total_faces * 3)); // We must triangulate the mesh from the modelling software!

//     return true;
// }
