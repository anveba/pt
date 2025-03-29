#include "scenebuild.h"

#include <iostream>
#include <optional>
#include <stdexcept>

#include "colour.h"
#include "io/ioutil.h"
#include "io/parseutil.h"
#include "io/tokenstream.h"
#include "light.h"

SceneBuilder::SceneBuilder()
{
}

SceneBuilder::~SceneBuilder()
{
}

struct SceneInstance
{
    Transform transform;
    PbrMaterial material;
};

struct Subscene
{
    std::string identifier, path;
    std::vector<SceneInstance> instances;
};

// A state that is passed around when parsing.
struct SceneParserState
{
    SceneParserState(const std::string data_dir)
        : data_dir(data_dir)
    {
        current_object = std::nullopt;
        current_transform = Mat4(1.0f);
        subscenes = std::vector<Subscene>();
        subscene_indices = std::unordered_map<std::string, uint32_t>();
        lights = std::vector<PointLight>();
        camera = std::nullopt;
    }

    SceneParserState(SceneParserState const&) = delete;
    void operator=(SceneParserState const&) = delete;

    std::string data_dir;

    std::optional<std::string> current_object;

    Mat4 current_transform;
    PbrMaterial current_material;

    std::vector<Subscene> subscenes;
    std::unordered_map<std::string, uint32_t> subscene_indices;

    std::vector<PointLight> lights;

    std::optional<Camera> camera;
};

// Assumes identifier is consumed in stream.
static void parse_identifier(const std::string& identifier,
                             TokenStream& tokens,
                             SceneParserState& state)
{
    // Identifier alone on a line means a new instance is being created.
    if (tokens.done()) {
        if (!state.subscene_indices.count(identifier))
            throw std::runtime_error("Object " + identifier + " not recognised");

        // If this is not the first instance in the file, we
        // finialise the previous instance
        if (state.current_object.has_value()) {
            SceneInstance instance{ .transform = state.current_transform,
                                    .material = state.current_material };
            state.subscenes[state.subscene_indices[state.current_object.value()]].instances.push_back(instance);
        }

        // Reset state for new instance
        state.current_object = identifier;
        state.current_transform = Mat4(1.0f);
        state.current_material.clear();

    } else {
        state.subscene_indices[identifier] = state.subscenes.size();
        Subscene s{ .identifier = identifier, .path = tokens.next() };
        state.subscenes.push_back(s);
    }
}

static bool is_section_label(const std::string& token)
{
    return token[token.size() - 1] == ':';
}

// Parses the camera section
static void parse_camera(TokenStream& lines, SceneParserState& state)
{
    // Default camera values
    Vec3 position(0.0f);
    Vec3 look_dir(0.0f, 0.0f, -1.0f);
    Vec3 up(0.0f, 1.0f, 0.0f);
    float fov = 45.0f;
    float near = 0.1f, far = 1000.0f;

    while (!lines.done()) {
        TokenStream tokens(lines.next(), ' ');

        if (tokens.done())
            continue;
        const std::string& tok = tokens.next();

        if (tok == "position") {
            position = parse_vector(tokens);
        } else if (tok == "lookdir") {
            look_dir = parse_vector(tokens);
        } else if (tok == "up") {
            up = parse_vector(tokens);
        } else if (tok == "fov") {
            fov = std::stof(tokens.next());
        } else if (tok == "near") {
            near = std::stof(tokens.next());
        } else if (tok == "far") {
            far = std::stof(tokens.next());
        } else if (is_section_label(tok)) {
            // New section has started, so we exit.
            lines.rollback(1);
            break;
        } else {
            throw std::runtime_error("Unrecognised token in camera section " + tok);
        }
    }

    Vec3 x = glm::normalize(look_dir);
    Vec3 y = glm::normalize(glm::cross(up, x));
    Vec3 z = glm::cross(x, y);

    state.camera = Camera(
        position,
        Quaternion(Mat3(y, z, x)),
        glm::radians(fov),
        1.0,
        near,
        far);
}

// Parses the objects section
static void parse_objects(TokenStream& lines, SceneParserState& state)
{
    while (!lines.done()) {
        TokenStream tokens(lines.next(), ' ');

        if (tokens.done())
            continue;
        const std::string& tok = tokens.next();

        if (tok == "t") {
            if (!state.current_object.has_value())
                throw std::runtime_error("No instance to translate");
            state.current_transform = parse_translation(tokens) * state.current_transform;
        } else if (tok == "r") {
            if (!state.current_object.has_value())
                throw std::runtime_error("No instance to rotate");
            state.current_transform = parse_rotation(tokens) * state.current_transform;
        } else if (tok == "s") {
            if (!state.current_object.has_value())
                throw std::runtime_error("No instance to scale");
            state.current_transform = parse_scaling(tokens) * state.current_transform;
        } else if (tok == "basecol") {
            if (!state.current_object.has_value())
                throw std::runtime_error("No instance to apply base colour parameter to");
            // TODO
        } else if (tok == "roughness") {
            if (!state.current_object.has_value())
                throw std::runtime_error("No instance to apply roughness parameter to");
            // TODO
        } else if (tok == "metalness") {
            if (!state.current_object.has_value())
                throw std::runtime_error("No instance to apply metalness parameter to");
            // TODO
        } else if (tok == "specular") {
            if (!state.current_object.has_value())
                throw std::runtime_error("No instance to apply specular parameter to");
            // TODO
        } else if (is_section_label(tok)) {
            // New section has started, so we exit.
            lines.rollback(1);
            break;
        } else {
            parse_identifier(tok, tokens, state);
        }
    }

    // Since we only add an instance to the scene when it is made no longer
    // relevant by another instance, the last instance won't be added.
    // The following corrects for that.
    if (state.current_object.has_value()) {
        SceneInstance instance{ .transform = state.current_transform,
                                .material = state.current_material };
        state.subscenes[state.subscene_indices[state.current_object.value()]].instances.push_back(instance);
    }
}

// Parses on the section level (camera/objects)
static void parse_sections(TokenStream& lines, SceneParserState& state)
{
    while (!lines.done()) {
        TokenStream tokens(lines.next(), ' ');
        if (tokens.done())
            continue;
        const std::string& tok = tokens.next();
        if (tok == "camera:") {
            parse_camera(lines, state);
        } else if (tok == "objects:") {
            parse_objects(lines, state);
        } else {
            throw std::runtime_error("Unrecognised token " + tok);
        }
    }
}

void SceneBuilder::read_scene_description(Scene& scene, Camera& camera, const std::string& path)
{
    scene.clear();
    scene.resource_directory = directory_of(path);

    std::string chars_to_remove = "\r\t";
    std::string filtered = str_from_file(path);
    filter_string(filtered, chars_to_remove);

    TokenStream lines(filtered, '\n');

    SceneParserState state(scene.resource_directory);

    parse_sections(lines, state);

    if (!state.camera.has_value())
        throw std::runtime_error("No camera section in scene description");

    camera = state.camera.value();

    Camera dummy_camera(Vec3(0.0f), Quaternion(), 45.0f, 1.0f, 0.1f, 10000.0f); // TODO remove

    // TODO materials

    for (const Subscene& subscene : state.subscenes) {
        if (subscene.instances.empty())
            continue;

        size_t mat_offset = scene.materials.size();
        size_t tex_offset = scene.texture_paths.size();

        Scene loaded_scene;
        loaded_scene.from_file(scene.resource_directory + subscene.path, dummy_camera);

        // Add all object variants and instances from the subscenes and their instances
        for (const ObjectVariant& loaded_variant : loaded_scene.object_variants) {

            scene.object_variants.emplace_back();
            scene.object_variants.back().mesh = loaded_variant.mesh;
            scene.object_variants.back().instances.resize(loaded_variant.instances.size() * subscene.instances.size());

            size_t inst_idx = 0;
            for (const SceneInstance& scene_inst : subscene.instances) {

                for (const Instance& obj_inst : loaded_variant.instances) {
                    scene.object_variants.back().instances[inst_idx].material_index = obj_inst.material_index + mat_offset;
                    scene.object_variants.back().instances[inst_idx].transform.matrix = scene_inst.transform.matrix * obj_inst.transform.matrix;
                    inst_idx++;
                }
            }
            assert(inst_idx == loaded_variant.instances.size() * subscene.instances.size());
        }

        // Add all materials
        scene.materials.insert(scene.materials.end(), loaded_scene.materials.begin(), loaded_scene.materials.end());
        for (size_t i = mat_offset; i < scene.materials.size(); i++) {
            for (int j = 0; j < 4; j++) {
                if (scene.materials[i].col_emi_rgh_spec_maps[j] < UINT32_MAX)
                    scene.materials[i].col_emi_rgh_spec_maps[j] += tex_offset;
                if (scene.materials[i].shn_clcoat_metal_norm_maps[j] < UINT32_MAX)
                    scene.materials[i].shn_clcoat_metal_norm_maps[j] += tex_offset;
            }
        }

        // Add all textures
        std::string subscene_relative_resource_directory = directory_of(subscene.path);
        scene.texture_paths.insert(scene.texture_paths.end(), loaded_scene.texture_paths.begin(), loaded_scene.texture_paths.end());
        for (size_t i = tex_offset; i < scene.texture_paths.size(); i++) {
            scene.texture_paths[i] = subscene_relative_resource_directory + scene.texture_paths[i];
        }
    }

    assert(scene.check_valid());
}