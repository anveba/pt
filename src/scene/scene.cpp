#include "scene.h"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

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

static void process_materials(const aiScene* scene, std::vector<PbrMaterial>& materials)
{
    assert(scene->mNumMaterials > 0);
    materials.resize(scene->mNumMaterials);

    for (size_t i = 0; i < scene->mNumMaterials; i++) {

        const auto mat = scene->mMaterials[i];
        aiColor3D base_colour, emission;
        float roughness, specular;
        if (mat->Get(AI_MATKEY_BASE_COLOR, base_colour) != aiReturn_SUCCESS)
            std::cout << "no base colour" << std::endl;
        if (mat->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness) != aiReturn_SUCCESS)
            std::cout << "no roughness" << std::endl;
        if (mat->Get(AI_MATKEY_COLOR_EMISSIVE, emission) != aiReturn_SUCCESS)
            std::cout << "no emission" << std::endl;
        if (mat->Get(AI_MATKEY_SPECULAR_FACTOR, specular) != aiReturn_SUCCESS)
            std::cout << "no specular" << std::endl;
        materials[i].base_colour = Vec4(base_colour.r, base_colour.g, base_colour.b, roughness);
        materials[i].emission = Vec4(emission.r, emission.g, emission.b, specular);
    }
}

static void process_meshes(const aiScene* scene, std::vector<ObjectVariant>& object_variants)
{
    object_variants.resize(scene->mNumMeshes);

    for (size_t i = 0; i < scene->mNumMeshes; i++) {

        const auto mesh = scene->mMeshes[i];
        object_variants[i].mesh.get_vertices().resize(mesh->mNumVertices);

        bool has_uvs = mesh->GetNumUVChannels() != 0;
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

    process_materials(scene, materials);

    process_meshes(scene, object_variants);

    process_scene_node(object_variants, camera, scene, scene->mRootNode, Mat4(1.0f));

    size_t instance_count = 0, unique_vertex_count = 0, unique_triangle_count = 0, triangle_count = 0;
    for (const ObjectVariant& variant : object_variants) {
        instance_count += variant.instances.size();
        unique_vertex_count += variant.mesh.get_vertices().size();
        unique_triangle_count += variant.mesh.get_indexed_triangles().size();
        triangle_count += variant.instances.size() * variant.mesh.get_indexed_triangles().size();
    }
    std::cout << "Scene \"" << scene->mName.C_Str() << "\" has:\n"
              << "    " << scene->mNumCameras << " cameras\n"
              << "    " << object_variants.size() << " meshes\n"
              << "    " << instance_count << " instances\n"
              << "    " << unique_vertex_count << " unique vertices\n"
              << "    " << unique_triangle_count << " unique triangles\n"
              << "    " << triangle_count << " total triangles\n"
              << std::endl;
}

void Scene::clear()
{
    object_variants.clear();
    point_lights.clear();
    materials.clear();
}
