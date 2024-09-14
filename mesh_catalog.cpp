#include "mesh_catalog.h"

//
// Todo:
// - // @Cleanup: Make one big outer loop of this and everything above.
//

/*
#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
*/

#include <stb_image.h>

#include "file_utils.h"
#include "opengl.h"
#include "main.h"

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
           triangle_mesh->vertex_frames.count        == 0 &&
           triangle_mesh->canonical_vertex_map.count == 0);

    triangle_mesh->vertices             = NewArray<Vector3>(num_vertices);
    triangle_mesh->uvs                  = NewArray<Vector2>(num_vertices);
    triangle_mesh->vertex_frames        = NewArray<Frame3>(num_vertices);
    triangle_mesh->canonical_vertex_map = NewArray<i32>(num_vertices);
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

//
// I don't think we should load all the mesh into memory at the start,
// as this will bog down the load time of the whole game.
// Instead, for meshes, we should load them on demand of the levels.
//
bool load_mesh_into_memory(Triangle_Mesh *mesh)
{
    assert((mesh->full_path));
    auto full_path   = mesh->full_path;
    auto c_path      = (char*)(temp_c_string(full_path));
    auto extension   = find_character_from_right(full_path, '.');
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
RArr<Texture_Map*> textures_lookup; // The textures stored in here are either: a) Loaded from the embedded model file, or b) Retrieved using the texture catalog.

void init_mesh_catalog(Mesh_Catalog *catalog)
{
    catalog->base.my_name = String("Triangle Mesh");

    // array_add(&catalog->base.extensions, String("obj"));

    // array_add(&catalog->base.extensions, String("fbx"));
    array_add(&catalog->base.extensions, String("gltf"));
    // array_add(&catalog->base.extensions, String("glb"));

    textures_lookup.allocator = {global_context.temporary_storage, __temporary_allocator};

    do_polymorphic_catalog_init(catalog);
}

Triangle_Mesh *make_placeholder(Mesh_Catalog *catalog, String short_name, String full_name)
{
    auto mesh       = New<Triangle_Mesh>(false);
    mesh->name      = copy_string(short_name);
    mesh->full_path = copy_string(full_name);
    mesh->dirty     = true;

    return mesh;
}

void reload_asset(Mesh_Catalog *catalog, Triangle_Mesh *mesh)
{
    if (mesh->loaded)
    {
        // @Leak: Not freeing the thing!!!
        logprint("mesh_catalog", "Has not made a free_mesh() procedure for hotloading meshes!\n");
        assert(0);
    }

    auto load_success = load_mesh_into_memory(mesh);
    if (!load_success)
    {
        logprint("mesh_catalog", "Was not able to load the mesh '%s' into memory!\n", temp_c_string(mesh->full_path));
        return;
    }

    // Make vertex buffer for the mesh.
    {
        auto count       = mesh->vertices.count;
        auto dest_buffer = NewArray<Vertex_XCNUU>(count);

        i64 it_index = 0;
        for (auto &dest : dest_buffer)
        {
            // Every mesh supposed to have a vertex, but what the heyy, consistency!!?!?!?
            if (mesh->vertices) dest.position = mesh->vertices[it_index];
            else dest.position = Vector3(0, 0, 0);

            if (mesh->colors)
            {
                auto c = mesh->colors[it_index];
                c.w = 1.0f; // Just to make sure, although we should deprecate the color scale way of rendering things soon. :DeprecateMe.

                dest.color_scale = argb_color(c);
            }
            else dest.color_scale = 0xffffffff;

            if (mesh->vertex_frames) dest.normal = mesh->vertex_frames[it_index].normal;
            else dest.normal = Vector3(1, 0, 0);

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

    assert(0);
    return NULL;
}

inline
void read_normals_from_accessor(u8 *dest, cgltf_accessor *accessor, i32 offset, i32 strides_as_amount_of_floats)
{
    assert(accessor);
    assert(accessor->buffer_view);
    assert(!accessor->is_sparse);

    auto element = cgltf_buffer_view_data(accessor->buffer_view);
    assert(element);
    element += accessor->offset;

    auto floats_per_element = cgltf_num_components(accessor->type);
    assert((accessor->component_type == cgltf_component_type_r_32f) && (accessor->stride == floats_per_element * sizeof(f32)));

    auto input_data  = reinterpret_cast<f32*>(const_cast<u8*>(element));
    auto output_data = reinterpret_cast<f32*>(dest + offset);

    for (i64 i = 0; i < accessor->count; ++i)
    {
        auto value = input_data + (i * floats_per_element);
        output_data[0] = value[0];
        output_data[1] = value[1];
        output_data[2] = value[2];

        output_data += strides_as_amount_of_floats;
    }
}

inline
void read_frame3_from_accessor(u8 *dest, i32 total_vertices, cgltf_accessor *normals_accessor, cgltf_accessor *tangents_accessor)
{
    assert(normals_accessor);

    //
    // The accessor in GLTF only has data for normals and tangents, so we load both of
    // them and calculate the bitangent ourselves.
    //
    // @Speed: Totally unoptimized.
    //

    assert(total_vertices == normals_accessor->count);

    auto normals_offset = offsetof(Frame3, normal);
    auto frame3_stride_in_floats = sizeof(Frame3) / sizeof(f32);
    read_normals_from_accessor(dest, normals_accessor, normals_offset, frame3_stride_in_floats);

    if (tangents_accessor)
    {
        // If you have tangents, you better have the right amount.
        assert(total_vertices == tangents_accessor->count);

        //
        // We are not using read_floats_with_offsets_from_accessor, because
        // the tangents we are reading from contains 4 elements, while we
        // only care about three, and the last one is just the scalar for the
        // Vector3 made up of the first 3. So we are @Cutnpaste'ing again.
        //
        auto accessor = tangents_accessor;
        assert(accessor->buffer_view);
        assert(!accessor->is_sparse);

        auto element = cgltf_buffer_view_data(accessor->buffer_view);
        assert(element);
        element += accessor->offset;

        auto floats_per_element = cgltf_num_components(accessor->type);
        assert((accessor->component_type == cgltf_component_type_r_32f) && (accessor->stride == floats_per_element * sizeof(f32)));

        auto input_data = reinterpret_cast<f32*>(const_cast<u8*>(element));

        auto tangents_offset = offsetof(Frame3, tangent);
        auto tangents_output = reinterpret_cast<f32*>(dest + tangents_offset);

        auto bitangents_offset = offsetof(Frame3, bitangent);
        auto bitangents_output = reinterpret_cast<f32*>(dest + bitangents_offset);

        auto normals_lookup = reinterpret_cast<f32*>(dest + normals_offset);

        for (i64 i = 0; i < accessor->count; ++i)
        {
            auto value = input_data + (i * floats_per_element);
            auto t = Vector3(value[0], value[1], value[2]) * value[3];

            tangents_output[0] = t.x;
            tangents_output[1] = t.y;
            tangents_output[2] = t.z;

            auto norm  = Vector3(normals_lookup[0], normals_lookup[1], normals_lookup[2]);
            auto bitan = glm::cross(norm, t);
            bitangents_output[0] = bitan.x;
            bitangents_output[1] = bitan.y;
            bitangents_output[2] = bitan.z;

            tangents_output   += frame3_stride_in_floats;
            normals_lookup    += frame3_stride_in_floats;
            bitangents_output += frame3_stride_in_floats;
        }
    }
}

inline
bool read_floats_from_accessor(u8 *dest, cgltf_accessor *accessor)
{
    if (!accessor) return false;

    const auto FLOATS_COUNT = accessor->count * cgltf_num_components(accessor->type);
    auto unpacked_amount = cgltf_accessor_unpack_floats(accessor, reinterpret_cast<f32*>(dest), FLOATS_COUNT);
    assert(unpacked_amount == FLOATS_COUNT); // @Cleanup: Do error logging instead of fat assert?

    return true;
}

//
// Given an accessor (in GLTF, this is the main way to read data), we load the
// copy the content of that accessor (we are not allocating any data, just assigning
// the pointer elsewhere) to a given memory address.
// If the count or type address(es) is provided, we also store the count of the accessor
// and the type of it.
//
template <typename T>
inline                       // @Cleanup: Replace the calls to read_*_from_accessor with this.
void load_accessor_into_memory_buffer(cgltf_accessor *accessor, T **memory_buffer_address, i64 *desired_count = NULL, cgltf_type *desired_type = NULL)
{
    assert(accessor->buffer_view && accessor->buffer_view->buffer);

    auto gltf_buffer   = accessor->buffer_view->buffer;
    auto buffer_offset = accessor->offset + accessor->buffer_view->offset;

    *memory_buffer_address = reinterpret_cast<T*>(reinterpret_cast<u8*>(gltf_buffer->data) + buffer_offset);

    if (desired_count) *desired_count = accessor->count;    
    if (desired_type)  *desired_type  = accessor->type;
}

bool load_gltf_model_2024(Triangle_Mesh *triangle_mesh) // @Cleanup: Rename
{
    // @Fixme Currently @Leak if reload.

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

    // We need:
    // - Total vertices.
    // - Total faces.
    // - Total triangle list info, which is the number of sub meshes.
    // - Need to determine if the model is using colors or textures?
    // 
    // - To allocate the space for the triangle mesh, materials, and color/texture data.

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

    //
    // Pass 1: Get total number of vertices.
    //
    for (i32 it_index = 0; it_index < total_triangle_list_info; ++it_index)
    {
        auto sub_mesh = &parsed_gltf_data->meshes[it_index];

        auto total_primitives = sub_mesh->primitives_count;
        assert((total_primitives == 1)); // @Incomplete: Not handling multiple primitives per mesh.

        auto primitive_index = 0; // Only dealing with the first one primitive!
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

        use_colors = use_colors || (colors_accessor != NULL);
    }

    total_faces /= 3; // We divide the total faces by 3 because total_faces = total_indices / 3. Assuming that the mesh is triangulated.

    // printf("Vertices count for model '%s' is %d\n", c_path, total_vertices);
    // printf("Faces count for model '%s' is %d\n", c_path, total_faces);

    //
    // Allocating space for the vertices, indices, triangle list infos, and materials.
    //
    allocate_geometry(triangle_mesh, total_vertices, total_faces);
    allocate_triangle_list_info(triangle_mesh, total_triangle_list_info);

    if (!(total_vertices && total_faces))
    {
        logprint("gltf_loader", "ERROR: Model '%s' contains no vertices or faces.\n", c_path);
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
            auto texture_map  = catalog_find(&texture_catalog, texture_name);

            if (texture_map)
            {
                // printf("Texture map '%s' was already loaded!\n", temp_c_string(texture_map->name));
                textures_lookup[gltf_image_index] = texture_map;
                continue;
            }

            if (gltf_image->uri)
            {
                // @Important: We should always remember to export the models with textures relative to its location.
                auto texture_path_relative_to_exe = tprint(String("%s/%s"), temp_c_string(relative_model_path), gltf_image->uri);

                my_register_loose_file<Texture_Map>(&texture_catalog.base, texture_name, texture_path_relative_to_exe);
                textures_lookup[gltf_image_index] = catalog_find(&texture_catalog, texture_name); // NULL means look into the texture catalog and find me.
            }
            else
            {
                auto image_buffer_view = gltf_image->buffer_view;
                if (image_buffer_view)
                {
                    // Getting the width, height, and number of channels.
                    auto image_buffer = image_buffer_view->buffer; 
                    i32 width, height, channels_count;

                    i64 size_to_read = image_buffer_view->size;
                    u8 *src  = reinterpret_cast<u8*>(image_buffer->data) + image_buffer_view->offset;
                    u8 *data = stbi_load_from_memory(src, size_to_read, &width, &height, &channels_count, 4);

                    if (!data)
                    {
                        logprint("gltf_loader", "Failed to load embedded image '%s' of model '%s' from memory!\n", texture_name.data, c_path);
                        return false;
                    }

                    // Get minification and magnification filters.
                    // There might be duplicated textures of the same image and samplers,
                    // so what we do is to loop from the image index and start probing from there.
                    auto corresponding_sampler = get_sampler_of_image(parsed_gltf_data, gltf_image_index, gltf_image);

                    // These values corresponds to the GL_* flag to set for GL_TEXTURE_MIN_FILTER, *_MAG_FILTER, *_WRAP_S, *_WRAP_T.
                    auto min_filter = corresponding_sampler->min_filter; // @Incomplete: Attribute is not used in set_texture()
                    auto mag_filter = corresponding_sampler->mag_filter; // @Incomplete: Attribute is not used in set_texture()
                    auto wrap_s = corresponding_sampler->wrap_s; // @Incomplete: Attribute is not used in set_texture()
                    auto wrap_t = corresponding_sampler->wrap_t; // @Incomplete: Attribute is not used in set_texture()

                    // This is wrong, clean it up later!!! It's wrong because we should not load more than one texture and there should be a create texture function for this.
                    {
                        auto resulting_map = make_placeholder(&texture_catalog, texture_name, model_path_relative_to_exe); // Using the path relative to exe for the texture's full path because it is embedded.
                        resulting_map->width  = width;
                        resulting_map->height = height;

                        auto bitmap = New<Bitmap>(false);
                        bitmap->width  = width;
                        bitmap->height = height;
                        bitmap->data   = data;
                        bitmap->length_in_bytes = width * height * channels_count;

                        // @Temporary @Hack @Fixme @Incomplete @Cutnpaste from texture_catalog
                        if (channels_count == 3) bitmap->format = Texture_Format::RGB888;
                        else                     bitmap->format = Texture_Format::ARGB8888;

                        resulting_map->data = bitmap;
                        update_texture(resulting_map);

                        textures_lookup[gltf_image_index] = resulting_map;
                    }
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

            auto skeleton_data = &parsed_gltf_data->skins[0]; // Using the first skeleton, always.
            auto inverse_bind_matrices_accessor = skeleton_data->inverse_bind_matrices;

            printf("Loading skeleton named '%s' for model '%s'!\n", skeleton_data->name, c_path);

            if (inverse_bind_matrices_accessor)
            {
                // Only here we allocate the space for the skeleton info, because if the skeleton
                // does not contain some inverse bind matrices, then I don't know what to do...
                assert(triangle_mesh->skeleton_info == NULL);
                auto info = New<Skeleton_Info>();

                auto nodes_count    = skeleton_data->joints_count;
                auto vertices_count = triangle_mesh->vertices.count; // Must allocate the vertices before doing skinning info.

                info->skeleton_node_info = NewArray<Skeleton_Node_Info>(nodes_count);
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
                assert(matrices_count == nodes_count);

                for (i64 it_index = 0; it_index < nodes_count; ++it_index)
                {
                    auto gltf_node = skeleton_data->joints[it_index];

                    //
                    // This is the index of the node within the whole index pool.
                    // We use this index when we refer to any node's index.
                    // This is very important, as we don't refer to the it_index.
                    //
                    auto node_index_globally = cgltf_node_index(parsed_gltf_data, gltf_node);

                    // I don't know if this is right or not.
                    auto node = &info->skeleton_node_info[node_index_globally];
                    node->name = copy_string(String(gltf_node->name));

                    Matrix4 m = matrices_memory[it_index]; // @Investigate:
                    node->rest_object_space_to_object_space = m;
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
            auto material = &triangle_mesh->material_info[material_index];
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
                    logprint("Material stuff", "Material does not contains a texture image, using the white texture with the base color!"); // untested @Investigate

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
                    logprint("Material stuff", "In material '%s':\n", gltf_material->name);
                    logprint("Material stuff", "   - texcoord = %d\n", metallic_roughness_texture->texcoord);
                    logprint("Material stuff", "   - scale = %f\n", metallic_roughness_texture->scale);
                    logprint("Material stuff", "   - has_transform = %d\n", metallic_roughness_texture->has_transform);

                    auto image = metallic_roughness_texture->texture->image;
                    if (image)
                    {
                        logprint("Material stuff", "   - Image name is '%s'!\n", image->name);
                    }

                    // @Incomplete: Figure out what this does.
                    assert(0);
                }

                // The normal map.
                auto normal_texture = &gltf_material->normal_texture;
                if (normal_texture->texture)
                {
                    auto gltf_normal_index = cgltf_image_index(parsed_gltf_data, normal_texture->texture->image);
                    auto gltf_normal_name  = normal_texture->texture->image->name;
                    auto normal_map = textures_lookup[gltf_normal_index];

                    assert(normal_map);

                    logprint("Material stuff", "Normal map '%s' for material '%s'\n", gltf_normal_name, gltf_material->name);
                    auto normal_map_intensity = normal_texture->scale;
                    logprint("Material stuff", "Normal map intensity is %f\n", normal_map_intensity);

                    // @Incomplete: How to use normal maps?

                    // material->normal_2024 = normal_map;
                    // material->normal_intensity_2024 = normal_map_intensity;
                }

                // The ambient occlusion map.
                auto ao_texture = &gltf_material->occlusion_texture;
                if (ao_texture->texture)
                {
                    auto gltf_ao_name = ao_texture->texture->image->name;
                    logprint("Material stuff", "Material '%s' has ambient occlusion texture '%s'!\n", gltf_material->name, gltf_ao_name);

                    assert(0); // @Investigate: the 'red-guy.gltf' model should contains AO texture because in the file it definitely shows that there are AO textures.
                }
            }

            if (gltf_material->has_pbr_specular_glossiness)
            {
                logprint("Material stuff", "Material '%s' has uses the pbr specular glossiness workflow, not supported yet!\n", gltf_material->name);
                assert(0); // @Incomplete:
            }

            if (gltf_material->has_specular)
            {
                // printf("Material '%s' has pbr specular!\n", gltf_material->name);

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

            // if (gltf_material->has_ior)
            // {
            //     printf("Material '%s' has ior!\n", gltf_material->name);
            //     assert(0);
            // }
        }
    }

    //
    // Pass 5: Process the triangle list infos
    //
    i32 vertices_previous_lists = 0;
    i32 indices_previous_lists = 0;
    for (i32 it_index = 0; it_index < total_triangle_list_info; ++it_index)
    {
        auto sub_mesh = &parsed_gltf_data->meshes[it_index];

        auto total_primitives = sub_mesh->primitives_count;
        // @Incomplete: Again, same as the above comment, we are handling only the the first primitive.
        assert((total_primitives == 1));

        auto primitive_index = 0; // Only dealing with the first one primitive!
        auto primitive = &sub_mesh->primitives[primitive_index];

        //
        // Getting the accessors to the positions, colors, textures,...
        // so that we can load the values from there to the Triangle_Mesh.
        //
        cgltf_accessor *positions_accessor = NULL; // @Cleanup: Use the load_accessor_*()
        cgltf_accessor *normals_accessor   = NULL; // @Cleanup: Use the load_accessor_*()
        cgltf_accessor *tangents_accessor  = NULL; // @Cleanup: Use the load_accessor_*()
        cgltf_accessor *textures_accessor  = NULL; // @Cleanup: Use the load_accessor_*()
        cgltf_accessor *colors_accessor    = NULL; // @Cleanup: Use the load_accessor_*()

        u32 *bone_influences_memory = NULL;
        f32 *vertex_weights_memory  = NULL;

        i32 vertices_this_list = 0;
        for (i32 attrib_index = 0; attrib_index < primitive->attributes_count; ++attrib_index)
        {
            auto attrib   = &primitive->attributes[attrib_index];
            auto accessor = attrib->data;

            switch (attrib->type)
            {
                // Reading the vertices for this sub-mesh.
                case cgltf_attribute_type_position: {
                    if (!positions_accessor && (accessor->type == cgltf_type_vec3))
                    {
                        vertices_this_list = accessor->count;
                        positions_accessor = accessor;
                    }
                } break;
                case cgltf_attribute_type_normal: {
                    if (!normals_accessor && (accessor->type == cgltf_type_vec3)) normals_accessor = accessor;
                } break;
                case cgltf_attribute_type_tangent: {
                    if (!tangents_accessor && (accessor->type == cgltf_type_vec4)) tangents_accessor = accessor;
                } break;
                case cgltf_attribute_type_color: {
                    if (!colors_accessor && (accessor->type == cgltf_type_vec3)) colors_accessor = accessor;
                } break;
                case cgltf_attribute_type_texcoord: {
                    if (!textures_accessor && (accessor->type == cgltf_type_vec2)) textures_accessor = accessor;
                } break;
                case cgltf_attribute_type_joints: {
                    if (!bone_influences_memory) load_accessor_into_memory_buffer(accessor, &bone_influences_memory);

                    assert(cgltf_num_components(accessor->type) <= MAX_MATRICES_PER_VERTEX); // Should not exceed the maximum amount otherwise, we are in trouble.

                    auto component_type = accessor->component_type;
                    assert((component_type == cgltf_component_type_r_8) || (component_type == cgltf_component_type_r_8u));
                } break;
                case cgltf_attribute_type_weights: {
                    if (!vertex_weights_memory) load_accessor_into_memory_buffer(accessor, &vertex_weights_memory);

                    assert(accessor->component_type == cgltf_component_type_r_32f);
                } break;
            }
        }

        //
        // Process the positions, uvs, normals, tangents, bitangents, and colors.
        //
        {
            auto v_offset = vertices_previous_lists;

            // Positions.
            {
                u8 *dest = reinterpret_cast<u8*>(&triangle_mesh->vertices[v_offset]);
                auto success = read_floats_from_accessor(dest, positions_accessor);
                if (!success)
                {
                    logprint("gltf_loader", "Warning sub-mesh %d in model '%s' does not have any vertex positions!\n", it_index, c_path);
                }
            }

            // Frame3.
            {
                u8 *dest = reinterpret_cast<u8*>(&triangle_mesh->vertex_frames[v_offset]);
                read_frame3_from_accessor(dest, vertices_this_list, normals_accessor, tangents_accessor);
            }

            if (!tangents_accessor)
            {
                // @Incomplete: Supposed to calculate the tangents and bitangents based on the normals and vertices...
            }

            // Texture UVs.
            {
                // 
                // @Speed: Becase we load flipped textures by default, so here,
                // we cannot do memcpy. Instead, we must flip the y coordinate
                // of each individual elements.
                //

                auto accessor = textures_accessor;

                assert(accessor);
                assert(accessor->buffer_view);
                assert(!accessor->is_sparse);
                
                auto element = cgltf_buffer_view_data(accessor->buffer_view);
                assert(element);
                element += accessor->offset;

                auto floats_per_element = cgltf_num_components(accessor->type);
                assert((accessor->component_type == cgltf_component_type_r_32f) && (accessor->stride == floats_per_element * sizeof(f32)));

                auto input_data = reinterpret_cast<f32*>(const_cast<u8*>(element));

                u8 *dest = reinterpret_cast<u8*>(&triangle_mesh->uvs[v_offset]);
                auto output_data = reinterpret_cast<f32*>(dest);

                for (i64 i = 0; i < accessor->count; ++i)
                {
                    auto value = input_data + (i * floats_per_element);
                    output_data[0] = value[0];
                    output_data[1] = 1 - value[1];

                    output_data += floats_per_element;
                }
            }

            // Colors.
            if (use_colors)
            {
                if (colors_accessor)
                {
                    assert(colors_accessor->count == vertices_this_list);

                    auto accessor = colors_accessor;                    
                    assert(accessor->buffer_view);
                    assert(!accessor->is_sparse);

                    auto element = cgltf_buffer_view_data(accessor->buffer_view);
                    assert(element != NULL);
                    element += accessor->offset;

                    auto floats_per_element = cgltf_num_components(accessor->type);
                    assert((accessor->component_type == cgltf_component_type_r_32f) && (accessor->stride == floats_per_element * sizeof(f32)));

                    auto input_data = reinterpret_cast<f32*>(const_cast<u8*>(element));

                    u8 *dest = reinterpret_cast<u8*>(&triangle_mesh->colors[v_offset]);
                    auto output_data = reinterpret_cast<f32*>(dest);

                    const auto colors_stride_in_floats = sizeof(Vector4) / sizeof(f32);
                    for (i64 i = 0; i < accessor->count; ++i)
                    {
                        auto value = input_data + (i * floats_per_element);
                        output_data[0] = value[0];
                        output_data[1] = value[1];
                        output_data[2] = value[2];
                        output_data[3] = 1.0f;

                        output_data += colors_stride_in_floats;
                    }
                }
                else
                {
                    for (auto i = 0; i < vertices_this_list; ++i)
                    {
                        auto c = &triangle_mesh->colors[i];
                        c->x = c->y = c->z = c->w = 1.0f;
                    }
                }
            }

            // Bones and vertex weights
            if (bone_influences_memory && vertex_weights_memory)
            {
                assert(triangle_mesh->skeleton_info->vertex_blend_info);

                // @Hardcoded: We are doing 4 bones influence right now...
                // @Cleanup: Make one big outer loop of this and everything above.

                for (i32 i = 0; i < vertices_this_list; ++i)
                {
                    auto ids     = &reinterpret_cast<u8*>(bone_influences_memory)[i * 4];
                    auto weights = &vertex_weights_memory[i * 4];

                    auto blend_info   = &triangle_mesh->skeleton_info->vertex_blend_info[v_offset + i];
                    auto num_matrices = 0;

                    for (i32 j = 0; j < 4; ++j)
                    {
                        if (!weights[j]) continue; // If weight is 0, then that matrix doesn't influence the vertex.

                        auto piece = &blend_info->pieces[num_matrices];
                        piece->matrix_index  = static_cast<i32>(ids[j]);
                        piece->matrix_weight = weights[j];

                        num_matrices += 1;
                    }
                    
                    blend_info->num_matrices = num_matrices;
                }
            }

            // @Incomplete: Not handling texture color... or should we??
        }

        auto triangle_list_info = &triangle_mesh->triangle_list_info[it_index];

        //
        // Process the indices for this sub-mesh.
        //
        {
            auto indices_accessor  = primitive->indices;
            auto indices_this_list = indices_accessor->count;
            auto indices_offset    = indices_previous_lists;

            assert(!indices_accessor->is_sparse);
            assert(indices_accessor->buffer_view);

            triangle_list_info->num_indices = indices_this_list;
            triangle_list_info->first_index = indices_offset;

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
        // Assign the material to the triangle list.
        //
        auto material_this_list = primitive->material;
        if (material_this_list)
        {
            auto material_index = cgltf_material_index(parsed_gltf_data, material_this_list);
            auto material_info  = &triangle_mesh->material_info[material_index]; // Where we store the materials.

            triangle_list_info->material_index = material_index;
            triangle_list_info->map = material_info->albedo_2024;
        }
        else
        {
            logprint("gltf_loader", "Error: Sub-mesh '%s' has no materials,....\n", sub_mesh->name);
            // @Incomplete: Should be using the white texture or something here.
            assert(0);
        }

        // @Incomplete: Do bounding box!!!
        // @Incomplete: Do bounding box!!!
        // @Incomplete: Do bounding box!!!
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
