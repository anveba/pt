#include "scene.h"

#include "io/ioutil.h"
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <optional>
#include <sstream>

struct TextureMetadata
{
    uint32_t index;
    bool is_srgb;
};

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

static bool texture_is_srgb(aiTextureType texture_type)
{
    return texture_type == aiTextureType_BASE_COLOR ||
           texture_type == aiTextureType_DIFFUSE ||
           texture_type == aiTextureType_EMISSION_COLOR ||
           texture_type == aiTextureType_EMISSIVE;
}

// Returns the index of the texture.
static std::optional<uint32_t> process_texture(
    const aiMaterial* mat,
    aiTextureType texture_type,
    std::unordered_map<std::string, TextureMetadata>& texture_map,
    uint32_t& current_texture_index)
{
    if (mat->GetTextureCount(texture_type) == 0)
        return std::nullopt;
    aiString path;
    mat->GetTexture(texture_type, 0, &path);
    if (texture_map.count(path.C_Str()) == 0) {
        std::cout << "Using texture: " << path.C_Str() << std::endl;
        texture_map[path.C_Str()] = { .index = current_texture_index, .is_srgb = texture_is_srgb(texture_type) };
        return current_texture_index++;
    } else {
        return texture_map[path.C_Str()].index;
    }
}

static Vec4 process_base_colour(
    const aiMaterial* mat,
    uint32_t& map_bits,
    std::unordered_map<std::string, TextureMetadata>& texture_map,
    uint32_t& current_texture_index)
{
    std::optional<uint32_t> map = process_texture(mat, aiTextureType_BASE_COLOR, texture_map, current_texture_index);
    if (!map)
        map = process_texture(mat, aiTextureType_DIFFUSE, texture_map, current_texture_index);

    if (map) {
        map_bits |= BASE_COLOUR_MAP_BIT;
        float index_as_float;
        memcpy(&index_as_float, &map.value(), 4);
        return Vec4(index_as_float, 0.0f, 0.0f, 0.0f);
    } else {
        aiColor3D base_colour;
        if (mat->Get(AI_MATKEY_BASE_COLOR, base_colour) != aiReturn_SUCCESS)
            if (mat->Get(AI_MATKEY_COLOR_DIFFUSE, base_colour) != aiReturn_SUCCESS)
                base_colour = aiColor3D(1.0f, 1.0f, 1.0f);
        return Vec4(base_colour.r, base_colour.g, base_colour.g, 1.0); // TODO include opacity
    }
}

static Vec4 process_emission(
    const aiMaterial* mat,
    uint32_t& map_bits,
    std::unordered_map<std::string, TextureMetadata>& texture_map,
    uint32_t& current_texture_index)
{
    Vec4 result;

    std::optional<uint32_t> map = process_texture(mat, aiTextureType_EMISSION_COLOR, texture_map, current_texture_index);
    if (!map)
        map = process_texture(mat, aiTextureType_EMISSIVE, texture_map, current_texture_index);

    if (map) {
        map_bits |= EMISSION_MAP_BIT;
        float index_as_float;
        memcpy(&index_as_float, &map.value(), 4);
        result.x = index_as_float;
    } else {
        aiColor3D emission_colour;
        if (mat->Get(AI_MATKEY_COLOR_EMISSIVE, emission_colour) != aiReturn_SUCCESS)
            emission_colour = aiColor3D(0.0f, 0.0f, 0.0f);
        result.x = emission_colour.r;
        result.y = emission_colour.g;
        result.z = emission_colour.b;
    }

    float emission_intensity;
    if (mat->Get(AI_MATKEY_EMISSIVE_INTENSITY, emission_intensity) != aiReturn_SUCCESS)
        emission_intensity = 1.0f;
    result.a = emission_intensity;

    return result;
}

static Vec3 process_roughness_metalness_normal(
    const aiMaterial* mat,
    uint32_t& map_bits,
    std::unordered_map<std::string, TextureMetadata>& texture_map,
    uint32_t& current_texture_index)
{
    Vec3 result;

    std::optional<uint32_t> map = process_texture(mat, aiTextureType_GLTF_METALLIC_ROUGHNESS, texture_map, current_texture_index);
    if (map) {
        map_bits |= ROUGHNESS_METALNESS_MAP_BIT;
        float index_as_float;
        memcpy(&index_as_float, &map.value(), 4);
        result.x = index_as_float;
    } else {
        map = process_texture(mat, aiTextureType_DIFFUSE_ROUGHNESS, texture_map, current_texture_index);
        if (map) {
            map_bits |= ROUGHNESS_MAP_BIT;
            float index_as_float;
            memcpy(&index_as_float, &map.value(), 4);
            result.x = index_as_float;
        } else {
            float roughness;
            if (mat->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness) != aiReturn_SUCCESS)
                roughness = 0.5f;
            result.x = roughness;
        }

        map = process_texture(mat, aiTextureType_METALNESS, texture_map, current_texture_index);
        if (map) {
            map_bits |= METALNESS_MAP_BIT;
            float index_as_float;
            memcpy(&index_as_float, &map.value(), 4);
            result.y = index_as_float;
        } else {
            float metalness;
            if (mat->Get(AI_MATKEY_METALLIC_FACTOR, metalness) != aiReturn_SUCCESS)
                metalness = 0.5f;
            result.y = metalness;
        }
    }

    map = process_texture(mat, aiTextureType_NORMALS, texture_map, current_texture_index);
    if (map) {
        map_bits |= NORMAL_MAP_BIT;
        float index_as_float;
        memcpy(&index_as_float, &map.value(), 4);
        result.z = index_as_float;
    }

    return result;
}

static Vec4 process_specular(
    const aiMaterial* mat,
    uint32_t& map_bits,
    std::unordered_map<std::string, TextureMetadata>& texture_map,
    uint32_t& current_texture_index)
{
    Vec4 result;

    std::optional<uint32_t> map = process_texture(mat, aiTextureType_SPECULAR, texture_map, current_texture_index);

    if (map) {
        map_bits |= SPECULAR_MAP_BIT;
        float index_as_float;
        memcpy(&index_as_float, &map.value(), 4);
        result.x = index_as_float;
    } else {
        aiColor3D specular_colour;
        if (mat->Get(AI_MATKEY_SPECULAR_FACTOR, specular_colour) != aiReturn_SUCCESS)
            specular_colour = aiColor3D(0.5f, 0.5f, 0.5f);
        result.x = specular_colour.r;
        result.y = specular_colour.g;
        result.z = specular_colour.b;
    }

    return result;
}

static Vec4 process_sheen(
    const aiMaterial* mat,
    uint32_t& map_bits,
    std::unordered_map<std::string, TextureMetadata>& texture_map,
    uint32_t& current_texture_index)
{
    Vec4 result;

    std::optional<uint32_t> map = process_texture(mat, aiTextureType_SHEEN, texture_map, current_texture_index);

    if (map) {
        map_bits |= SHEEN_MAP_BIT;
        float index_as_float;
        memcpy(&index_as_float, &map.value(), 4);
        result.x = index_as_float;
    } else {
        aiColor3D sheen_colour;
        if (mat->Get(AI_MATKEY_SHEEN_COLOR_FACTOR, sheen_colour) != aiReturn_SUCCESS)
            sheen_colour = aiColor3D(0.0f, 0.0f, 0.0f);
        result.x = sheen_colour.r;
        result.y = sheen_colour.g;
        result.z = sheen_colour.b;
    }

    return result;
}

static Vec4 process_clearcoat_anisotropy(
    const aiMaterial* mat,
    uint32_t& map_bits,
    std::unordered_map<std::string, TextureMetadata>& texture_map,
    uint32_t& current_texture_index)
{
    Vec4 result;

    std::optional<uint32_t> map = process_texture(mat, aiTextureType_CLEARCOAT, texture_map, current_texture_index);

    if (map) {
        map_bits |= CLEARCOAT_MAP_BIT;
        float index_as_float;
        memcpy(&index_as_float, &map.value(), 4);
        result.x = index_as_float;
    } else {
        float clearcoat;
        if (mat->Get(AI_MATKEY_CLEARCOAT_FACTOR, clearcoat) != aiReturn_SUCCESS)
            clearcoat = 0.0f;
        result.x = clearcoat;
    }

    float cc_roughness;
    if (mat->Get(AI_MATKEY_CLEARCOAT_ROUGHNESS_FACTOR, cc_roughness) != aiReturn_SUCCESS)
        cc_roughness = 0.5f;
    result.y = cc_roughness;

    float anisotropy;
    if (mat->Get(AI_MATKEY_ANISOTROPY_FACTOR, anisotropy) != aiReturn_SUCCESS)
        anisotropy = 0.0f;
    result.z = anisotropy;

    return result;
}

static void process_camera(
    const aiScene* scene,
    Camera& camera)
{
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
}

void load_textures(const aiScene* scene, const std::unordered_map<std::string, TextureMetadata>& texture_map, std::vector<TextureData>& textures, const std::string& base_directory)
{
    for (const auto& t : texture_map) {
        auto embedded = scene->GetEmbeddedTextureAndIndex(t.first.c_str());
        if (embedded.first == nullptr) {
            textures[t.second.index] = std::move(TextureData(
                full_path(base_directory, t.first), t.second.is_srgb));
        } else {
            auto embedded_data = scene->mTextures[embedded.second];
            if (!strcmp(embedded_data->achFormatHint, "rgba8888")) {
                textures[t.second.index] = std::move(TextureData(
                    (uint8_t*)embedded_data->pcData,
                    embedded_data->mWidth * embedded_data->mHeight * 4,
                    embedded_data->mWidth,
                    embedded_data->mHeight,
                    t.second.is_srgb ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM));
            } else {
                throw std::runtime_error("Unsupported embedded texture format: " + std::to_string(embedded_data->mHeight));
            }
        }
    }
}

static void process_materials(
    const aiScene* scene,
    std::vector<PbrMaterial>& materials,
    std::vector<TextureData>& textures,
    const std::string& base_directory)
{
    assert(scene->mNumMaterials > 0);

    materials.resize(scene->mNumMaterials);

    std::unordered_map<std::string, TextureMetadata> texture_map;
    uint32_t current_texture_index = 0;

    for (size_t i = 0; i < scene->mNumMaterials; i++) {

        const aiMaterial* mat = scene->mMaterials[i];
        uint32_t map_bits = 0;

        materials[i].base_colour = process_base_colour(mat, map_bits, texture_map, current_texture_index);
        materials[i].emission = process_emission(mat, map_bits, texture_map, current_texture_index);
        Vec3 roughness_metalness_normal = process_roughness_metalness_normal(mat, map_bits, texture_map, current_texture_index);
        materials[i].specular = process_specular(mat, map_bits, texture_map, current_texture_index);
        materials[i].sheen = process_sheen(mat, map_bits, texture_map, current_texture_index);
        materials[i].cc_ccrgh_aniso = process_clearcoat_anisotropy(mat, map_bits, texture_map, current_texture_index);

        float map_bits_as_float;
        memcpy(&map_bits_as_float, &map_bits, 4);

        materials[i].rough_metal_normal_map_bits = Vec4(roughness_metalness_normal, map_bits_as_float);
    }

    textures.resize(texture_map.size());

    load_textures(scene, texture_map, textures, base_directory);
}

static void process_meshes(const aiScene* scene, std::vector<Mesh>& meshes, std::vector<ObjectVariant>& object_variants)
{
    // Takes advantage of the fact that Assimp meshes each have a material (meaning
    // that objects with the same mesh but differing materials won't exist).

    object_variants.resize(scene->mNumMeshes);
    meshes.resize(scene->mNumMeshes);

    for (size_t i = 0; i < scene->mNumMeshes; i++) {

        const auto mesh = scene->mMeshes[i];
        object_variants[i].mesh_index = i;
        object_variants[i].material_index = mesh->mMaterialIndex;
        meshes[i].get_vertices().resize(mesh->mNumVertices);

        bool has_uvs = mesh->mTextureCoords[0] != nullptr;
        if (!has_uvs)
            std::cerr << "Warning: mesh \"" << mesh->mName.C_Str() << "\" has no UVs." << std::endl;

        for (size_t j = 0; j < mesh->mNumVertices; j++) {
            Vertex& v = meshes[i].get_vertices()[j];
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

        meshes[i].get_indexed_triangles().resize(mesh->mNumFaces);
        for (size_t j = 0; j < mesh->mNumFaces; j++) {
            IndexedTriangle& t = meshes[i].get_indexed_triangles()[j];
            assert(mesh->mFaces->mNumIndices == 3);
            t.indices[0] = mesh->mFaces[j].mIndices[0];
            t.indices[1] = mesh->mFaces[j].mIndices[1];
            t.indices[2] = mesh->mFaces[j].mIndices[2];
        }
    }
}

static void process_lights(
    const aiScene* scene,
    std::vector<PointLight>& point_lights,
    std::vector<DirectionalLight>& directional_lights)
{

    for (size_t i = 0; i < scene->mNumLights; i++) {
        const aiLight* light = scene->mLights[i];
        if (light->mType == aiLightSource_POINT) {

            aiVector3D p = light->mPosition;
            aiColor3D c = light->mColorDiffuse;

            PointLight point_light(Vec3(p.x, p.y, p.z), Vec3(c.r, c.g, c.b));
            point_lights.push_back(point_light);

        } else if (light->mType == aiLightSource_DIRECTIONAL) {

            aiVector3D d = light->mDirection;
            aiColor3D c = light->mColorDiffuse;

            DirectionalLight directional_light(Vec3(d.x, d.y, d.z), Vec3(c.r, c.g, c.b));
            directional_lights.push_back(directional_light);
        }
    }
}

static void process_scene_node(std::vector<ObjectVariant>& variants, Camera& camera, const aiScene* scene, const aiNode* node, const Mat4& parent_transform)
{
    const Mat4 transform = parent_transform * convert_matrix(node->mTransformation);
    for (unsigned int i = 0; i < node->mNumMeshes; i++) {
        Instance inst;
        inst.transform.matrix = transform;

        variants[node->mMeshes[i]].instances.push_back(inst);
    }

    for (unsigned int i = 0; i < node->mNumChildren; i++)
        process_scene_node(variants, camera, scene, node->mChildren[i], transform);
}

std::string Scene::scene_details()
{
    size_t unique_vertex_count = 0, unique_triangle_count = 0;
    for (const Mesh& mesh : meshes) {
        unique_triangle_count += mesh.get_indexed_triangles().size();
        unique_vertex_count += mesh.get_vertices().size();
    }

    size_t instance_count = 0, triangle_count = 0, emitter_count = 0;
    for (const ObjectVariant& variant : get_object_variants()) {
        instance_count += variant.instances.size();
        triangle_count += variant.instances.size() * meshes[variant.mesh_index].get_indexed_triangles().size();
        emitter_count += (get_materials()[variant.material_index].is_emitter() ? 1 : 0) * variant.instances.size();
    }

    std::stringstream stream;
    stream << "Scene has:\n"
           << "    " << get_object_variants().size() << " meshes\n"
           << "    " << get_materials().size() << " materials\n"
           << "    " << point_lights.size() << " point lights\n"
           << "    " << directional_lights.size() << " directional lights\n"
           << "    " << emitter_count << " instances with emission\n"
           << "    " << get_textures().size() << " textures\n"
           << "    " << instance_count << " instances\n"
           << "    " << unique_vertex_count << " unique vertices\n"
           << "    " << unique_triangle_count << " unique triangles\n"
           << "    " << triangle_count << " total triangles\n";
    return stream.str();
}

size_t Scene::light_count() const
{
    size_t emitter_count = 0;
    for (size_t i = 0; i < get_object_variants().size(); i++) {

        const ObjectVariant& variant = get_object_variants()[i];

        if (get_materials()[variant.material_index].is_emitter())
            emitter_count += variant.instances.size();
    }
    return emitter_count + point_lights.size() + directional_lights.size();
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
            aiProcess_JoinIdenticalVertices |
            // aiProcess_ImproveCacheLocality |
            aiProcess_RemoveRedundantMaterials |
            aiProcess_FindDegenerates |
            aiProcess_FindInvalidData |
            aiProcess_FindInstances |
            aiProcess_OptimizeMeshes |
            aiProcess_OptimizeGraph);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
        throw std::runtime_error("Could not load scene at " + path + ": " + importer.GetErrorString());

    resource_directory = directory_of(path);

    process_camera(scene, camera);

    process_materials(scene, materials, textures, resource_directory);

    process_meshes(scene, meshes, object_variants);

    process_lights(scene, point_lights, directional_lights);

    process_scene_node(object_variants, camera, scene, scene->mRootNode, Mat4(1.0f));

    assert(check_valid());
}

void Scene::clear()
{
    object_variants.clear();
    point_lights.clear();
    materials.clear();
}

bool Scene::check_valid()
{
    for (const auto& variant : object_variants) {
        if (variant.material_index >= materials.size())
            return false;
        if (variant.mesh_index >= meshes.size())
            return false;
        // TODO check texture indices
    }

    return true;
}
