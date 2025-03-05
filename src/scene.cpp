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

static void process_scene_node(std::vector<ObjectVariant>& variants, aiNode* node, const Mat4& parent_transform)
{
    const Mat4 transform = parent_transform * convert_matrix(node->mTransformation);
    for (unsigned int i = 0; i < node->mNumMeshes; i++) {
        Instance inst;
        inst.transform.matrix = transform;

        variants[node->mMeshes[i]].instances.push_back(inst);
    }

    for (unsigned int i = 0; i < node->mNumChildren; i++)
        process_scene_node(variants, node->mChildren[i], transform);
}

void Scene::from_file(const std::string& path, Camera& camera)
{
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_GenNormals);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
        std::runtime_error("Could not load scene at " + path + ": " + importer.GetErrorString());

    std::cout << "Scene has " << scene->mNumCameras << " cameras." << std::endl;
    if (scene->mNumCameras > 0) {
        const auto& scene_cam = scene->mCameras[0];
        camera.position.x = scene_cam->mPosition.x;
        camera.position.y = scene_cam->mPosition.y;
        camera.position.z = scene_cam->mPosition.z;
        aiMatrix4x4 m;
        scene_cam->GetCameraMatrix(m);
        camera.rotation = Quaternion(convert_matrix(m));
        camera.aspect_ratio = scene_cam->mAspect;
        camera.near = scene_cam->mClipPlaneNear;
        camera.far = scene_cam->mClipPlaneFar;
    }

    object_variants.resize(scene->mNumMeshes);

    for (size_t i = 0; i < scene->mNumMeshes; i++) {

        const auto mesh = scene->mMeshes[i];
        object_variants[i].mesh.vertices.resize(mesh->mNumVertices);

        bool has_uvs = mesh->GetNumUVChannels() != 0;
        if (!has_uvs)
            std::cerr << "Warning: mesh \"" << mesh->mName.C_Str() << "\" has no UVs." << std::endl;

        for (size_t j = 0; j < mesh->mNumVertices; j++) {
            Vertex& v = object_variants[i].mesh.vertices[j];
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

        object_variants[i].mesh.tris.resize(mesh->mNumFaces);
        for (size_t j = 0; j < mesh->mNumFaces; j++) {
            IndexedTriangle& t = object_variants[i].mesh.tris[j];
            assert(mesh->mFaces->mNumIndices == 3);
            t.indices[0] = mesh->mFaces[j].mIndices[0];
            t.indices[1] = mesh->mFaces[j].mIndices[1];
            t.indices[2] = mesh->mFaces[j].mIndices[2];
        }
    }

    process_scene_node(object_variants, scene->mRootNode, Mat4(1.0f));
}