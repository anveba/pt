#include "scene.h"

#include "io/ioutil.h"
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <sstream>

static Mat4 convert_matrix(aiMatrix4x4 m)
{
    return Mat4(
        // Row 1
        m.a1,
        m.a2,
        m.a3,
        m.a4,
        // Row 2
        m.b1,
        m.b2,
        m.b3,
        m.b4,
        // Row 3
        m.c1,
        m.c2,
        m.c3,
        m.c4,
        // Row 4
        m.d1,
        m.d2,
        m.d3,
        m.d4);
}

static uint32_t process_texture(
    const aiMaterial* mat,
    aiTextureType texture_type,
    std::unordered_map<std::string, uint32_t>& texture_index_map,
    uint32_t& current_texture_index)
{
    if (mat->GetTextureCount(texture_type) == 0)
        return UINT32_MAX;
    aiString path;
    mat->GetTexture(texture_type, 0, &path);
    if (texture_index_map.count(path.C_Str()) == 0) {
        std::cout << "Using texture: " << path.C_Str() << std::endl;
        texture_index_map[path.C_Str()] = current_texture_index;
        return current_texture_index++;
    } else {
        return texture_index_map[path.C_Str()];
    }
}

static void process_materials(const aiScene* scene, std::vector<PbrMaterial>& materials, std::vector<std::string>& texture_paths)
{
    assert(scene->mNumMaterials > 0);

    materials.resize(scene->mNumMaterials);

    std::unordered_map<std::string, uint32_t> texture_index_map;
    uint32_t current_texture_index = 0;

    for (size_t i = 0; i < scene->mNumMaterials; i++) {

        const aiMaterial* mat = scene->mMaterials[i];
        aiColor3D base_colour, emission_colour;
        float roughness, emission_intensity, specular, metalness;
        bool includes_emission_data = true;
        if (mat->Get(AI_MATKEY_BASE_COLOR, base_colour) != aiReturn_SUCCESS)
            if (mat->Get(AI_MATKEY_COLOR_DIFFUSE, base_colour) != aiReturn_SUCCESS)
                base_colour = aiColor3D(1.0f, 1.0f, 1.0f);
        if (mat->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness) != aiReturn_SUCCESS)
            roughness = 1.0f;
        if (mat->Get(AI_MATKEY_COLOR_EMISSIVE, emission_colour) != aiReturn_SUCCESS) {
            emission_colour = aiColor3D(1.0f, 1.0f, 1.0f);
            includes_emission_data = false;
        }
        if (mat->Get(AI_MATKEY_SPECULAR_FACTOR, specular) != aiReturn_SUCCESS)
            specular = 1.0f;
        if (mat->Get(AI_MATKEY_METALLIC_FACTOR, metalness) != aiReturn_SUCCESS)
            metalness = 1.0f;
        materials[i].base_colour = Vec4(base_colour.r, base_colour.g, base_colour.b, roughness);
        materials[i].emission = Vec4(emission_colour.r, emission_colour.g, emission_colour.b, emission_intensity);
        materials[i].metalness_anisotropy = Vec4(metalness, 0.0f, 0.0f, 0.0f);

        materials[i].col_emi_rgh_spec_maps = Uint4(
            process_texture(mat, aiTextureType_BASE_COLOR, texture_index_map, current_texture_index),
            process_texture(mat, aiTextureType_EMISSION_COLOR, texture_index_map, current_texture_index),
            process_texture(mat, aiTextureType_DIFFUSE_ROUGHNESS, texture_index_map, current_texture_index),
            process_texture(mat, aiTextureType_SPECULAR, texture_index_map, current_texture_index));
        if (materials[i].col_emi_rgh_spec_maps.x == UINT32_MAX)
            materials[i].col_emi_rgh_spec_maps.x = process_texture(mat, aiTextureType_DIFFUSE, texture_index_map, current_texture_index);
        if (materials[i].col_emi_rgh_spec_maps.y == UINT32_MAX)
            materials[i].col_emi_rgh_spec_maps.y = process_texture(mat, aiTextureType_EMISSIVE, texture_index_map, current_texture_index);
        includes_emission_data = includes_emission_data || (materials[i].col_emi_rgh_spec_maps.y != UINT32_MAX);

        materials[i].shn_clcoat_metal_norm_maps = Uint4(
            process_texture(mat, aiTextureType_SHEEN, texture_index_map, current_texture_index),
            process_texture(mat, aiTextureType_CLEARCOAT, texture_index_map, current_texture_index),
            process_texture(mat, aiTextureType_METALNESS, texture_index_map, current_texture_index),
            process_texture(mat, aiTextureType_NORMALS, texture_index_map, current_texture_index));
        if (mat->Get(AI_MATKEY_EMISSIVE_INTENSITY, emission_intensity) != aiReturn_SUCCESS)
            emission_intensity = includes_emission_data ? 1.0f : 0.0f;
    }

    texture_paths.resize(texture_index_map.size());

    for (const auto& texture_index : texture_index_map)
        texture_paths[texture_index.second] = texture_index.first;
}

static void process_meshes(const aiScene* scene, std::vector<ObjectVariant>& object_variants)
{
    object_variants.resize(scene->mNumMeshes);

    for (size_t i = 0; i < scene->mNumMeshes; i++) {

        const auto mesh = scene->mMeshes[i];
        object_variants[i].mesh.get_vertices().resize(mesh->mNumVertices);

        bool has_uvs = mesh->mTextureCoords[0] != nullptr;
        if (!has_uvs)
            std::cerr << "Warning: mesh \"" << mesh->mName.C_Str() << "\" has no UVs." << std::endl;

        for (size_t j = 0; j < mesh->mNumVertices; j++) {
            Vertex& v = object_variants[i].mesh.get_vertices()[j];
            v.position.x = mesh->mVertices[j].x;
            v.position.y = mesh->mVertices[j].y;
            v.position.z = mesh->mVertices[j].z;
            v.normal.x = mesh->mNormals[j].x;
            v.normal.y = mesh->mNormals[j].y;
            v.normal.z = mesh->mNormals[j].z;
            if (has_uvs) {
                v.uv.x = mesh->mTextureCoords[0][j].x;
                v.uv.y = mesh->mTextureCoords[0][j].y;
            } else {
                v.uv = Vec2(0.0f);
            }
        }

        object_variants[i].mesh.get_indexed_triangles().resize(mesh->mNumFaces);
        for (size_t j = 0; j < mesh->mNumFaces; j++) {
            IndexedTriangle& t = object_variants[i].mesh.get_indexed_triangles()[j];
            assert(mesh->mFaces->mNumIndices == 3);
            t.indices[0] = mesh->mFaces[j].mIndices[0];
            t.indices[1] = mesh->mFaces[j].mIndices[1];
            t.indices[2] = mesh->mFaces[j].mIndices[2];
        }
    }
}

static void process_scene_node(std::vector<ObjectVariant>& variants, Camera& camera, const aiScene* scene, const aiNode* node, const Mat4& parent_transform)
{
    const Mat4 transform = parent_transform * convert_matrix(node->mTransformation);
    for (unsigned int i = 0; i < node->mNumMeshes; i++) {
        Instance inst;
        inst.transform.matrix = transform;
        inst.material_index = scene->mMeshes[node->mMeshes[i]]->mMaterialIndex;

        variants[node->mMeshes[i]].instances.push_back(inst);
    }

    for (unsigned int i = 0; i < node->mNumChildren; i++)
        process_scene_node(variants, camera, scene, node->mChildren[i], transform);
}

std::string Scene::scene_details()
{
    size_t instance_count = 0, unique_vertex_count = 0, unique_triangle_count = 0, triangle_count = 0;
    for (const ObjectVariant& variant : get_object_variants()) {
        instance_count += variant.instances.size();
        unique_vertex_count += variant.mesh.get_vertices().size();
        unique_triangle_count += variant.mesh.get_indexed_triangles().size();
        triangle_count += variant.instances.size() * variant.mesh.get_indexed_triangles().size();
    }
    std::stringstream stream;
    stream << "Scene has:\n"
           << "    " << get_object_variants().size() << " meshes\n"
           << "    " << get_materials().size() << " materials\n"
           << "    " << get_texture_paths().size() << " textures\n"
           << "    " << instance_count << " instances\n"
           << "    " << unique_vertex_count << " unique vertices\n"
           << "    " << unique_triangle_count << " unique triangles\n"
           << "    " << triangle_count << " total triangles\n";
    return stream.str();
}

void Scene::from_file(const std::string& path, Camera& camera)
{
    clear();

    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(
        path,
        aiProcess_Triangulate |
            aiProcess_FlipUVs |
            aiProcess_GenNormals |
            aiProcess_JoinIdenticalVertices); // TODO: take a look at post processing steps

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
        std::runtime_error("Could not load scene at " + path + ": " + importer.GetErrorString());

    resource_directory = directory_of(path);

    std::cout << "Found " << scene->mNumCameras << " camera(s) in scene." << std::endl;
    if (scene->mNumCameras > 0) {
        const auto& scene_cam = scene->mCameras[0];
        Mat4 transform = convert_matrix(scene->mRootNode->mTransformation * scene->mRootNode->FindNode(scene_cam->mName)->mTransformation);
        Vec4 pos = Vec4(scene_cam->mPosition.x, scene_cam->mPosition.y, scene_cam->mPosition.z, 1.0f);
        pos = transform * pos;
        camera.position = Vec3(pos);
        aiMatrix4x4 m;
        scene_cam->GetCameraMatrix(m);
        camera.rotation = Quaternion(convert_matrix(m));
        camera.aspect_ratio = scene_cam->mAspect;
        camera.near = scene_cam->mClipPlaneNear;
        camera.far = scene_cam->mClipPlaneFar;
        assert(camera.near > 0.0f);
        assert(camera.far > camera.near);
    }

    process_materials(scene, materials, texture_paths);

    process_meshes(scene, object_variants);

    process_scene_node(object_variants, camera, scene, scene->mRootNode, Mat4(1.0f));

    assert(check_valid());
}

void Scene::clear()
{
    object_variants.clear();
    point_lights.clear();
    materials.clear();
}

static bool texture_indices_are_valid(Uint4 indices, uint32_t size)
{
    return (indices.x < size || indices.x == UINT32_MAX) &&
           (indices.y < size || indices.y == UINT32_MAX) &&
           (indices.z < size || indices.z == UINT32_MAX) &&
           (indices.w < size || indices.w == UINT32_MAX);
}

bool Scene::check_valid()
{
    for (const auto& variant : object_variants) {
        for (const auto& inst : variant.instances) {
            if (inst.material_index >= materials.size())
                return false;
        }
    }

    for (const auto& mat : materials) {
        if (!texture_indices_are_valid(mat.col_emi_rgh_spec_maps, texture_paths.size()) ||
            !texture_indices_are_valid(mat.shn_clcoat_metal_norm_maps, texture_paths.size())) {
            return false;
        }
    }
    return true;
}
