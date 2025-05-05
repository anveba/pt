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
        , current_transform(1.0f)
        , environment_colour(0.0f)
    {
    }

    SceneParserState(SceneParserState const&) = delete;
    void operator=(SceneParserState const&) = delete;

    std::string data_dir;

    std::optional<std::string> current_object;

    Mat4 current_transform;
    PbrMaterial current_material;

    std::vector<Subscene> subscenes;
    std::unordered_map<std::string, uint32_t> subscene_indices;

    std::vector<PointLight> point_lights;
    std::vector<DirectionalLight> directional_lights;

    std::optional<Camera> camera;
    Vec3 environment_colour;
    std::string environment_map_path;
};

static void parse_point_light(TokenStream& tokens,
                              SceneParserState& state)
{
    if (tokens.size() - tokens.current_index() != 7)
        throw std::runtime_error("Invalid light syntax.");

    Vec3 pos = parse_vector(tokens);

    if (tokens.next() != ",")
        throw std::runtime_error("Invalid light syntax.");

    Vec3 col = from_srgb(parse_vector(tokens));

    state.point_lights.push_back(PointLight(pos, col));
}

static void parse_directional_light(TokenStream& tokens,
                                    SceneParserState& state)
{
    if (tokens.size() - tokens.current_index() != 7)
        throw std::runtime_error("Invalid light syntax.");

    Vec3 dir = parse_vector(tokens);

    if (tokens.next() != ",")
        throw std::runtime_error("Invalid light syntax");

    Vec3 col = from_srgb(parse_vector(tokens));

    state.directional_lights.push_back(DirectionalLight(dir, col));
}

// Assumes identifier is consumed in stream.
static void parse_identifier(const std::string& identifier,
                             TokenStream& tokens,
                             SceneParserState& state)
{
    // Identifier alone on a line means a new instance is being created.
    if (tokens.done()) {
        if (!state.subscene_indices.count(identifier))
            throw std::runtime_error("Object " + identifier + " not recognised.");

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
    if (state.camera)
        throw std::runtime_error("Camera already set.");

    // Default camera values
    Vec3 position(0.0f);
    Vec3 look_dir(0.0f, 0.0f, -1.0f);
    Vec3 up(0.0f, 1.0f, 0.0f);
    float fov = 45.0f;
    float near = 0.1f, far = 1000.0f;
    float focus_dist = 0.0f, lens_radius = 0.0f;
    float exposure = 1.0f;

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
        } else if (tok == "focusdist") {
            focus_dist = std::stof(tokens.next());
        } else if (tok == "lensradius") {
            lens_radius = std::stof(tokens.next());
        } else if (tok == "exposure") {
            exposure = std::stof(tokens.next());
        } else if (tok == "envcol") {
            state.environment_colour = from_srgb(parse_vector(tokens));
        } else if (tok == "envmap") {
            state.environment_map_path = tokens.next();
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
    state.camera.value().focus_dist = focus_dist;
    state.camera.value().lens_radius = lens_radius;
    state.camera.value().exposure = exposure;
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
                throw std::runtime_error("No instance to translate.");
            state.current_transform = parse_translation(tokens) * state.current_transform;
        } else if (tok == "r") {
            if (!state.current_object.has_value())
                throw std::runtime_error("No instance to rotate.");
            state.current_transform = parse_rotation(tokens) * state.current_transform;
        } else if (tok == "s") {
            if (!state.current_object.has_value())
                throw std::runtime_error("No instance to scale.");
            state.current_transform = parse_scaling(tokens) * state.current_transform;
        } else if (tok == "basecol") {
            if (!state.current_object.has_value())
                throw std::runtime_error("No instance to apply base colour parameter to.");
            // TODO
        } else if (tok == "roughness") {
            if (!state.current_object.has_value())
                throw std::runtime_error("No instance to apply roughness parameter to.");
            // TODO
        } else if (tok == "metalness") {
            if (!state.current_object.has_value())
                throw std::runtime_error("No instance to apply metalness parameter to.");
            // TODO
        } else if (tok == "specular") {
            if (!state.current_object.has_value())
                throw std::runtime_error("No instance to apply specular parameter to.");
            // TODO
        } else if (tok == "plight") {
            parse_point_light(tokens, state);
        } else if (tok == "dlight") {
            parse_directional_light(tokens, state);
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
        throw std::runtime_error("No camera section in scene description.");

    camera = state.camera.value();
    scene.environment_colour = state.environment_colour;
    scene.environment_map_path = state.environment_map_path;

    // Add lights that were described in the scene description file.
    scene.point_lights.insert(scene.point_lights.end(), state.point_lights.begin(), state.point_lights.end());
    scene.directional_lights.insert(scene.directional_lights.end(), state.directional_lights.begin(), state.directional_lights.end());

    // TODO materials

    Camera dummy_camera(Vec3(0.0f), Quaternion(), 45.0f, 1.0f, 0.1f, 10000.0f); // TODO remove

    for (const Subscene& subscene : state.subscenes) {
        if (subscene.instances.empty())
            continue;

        size_t mesh_offset = scene.meshes.size();
        size_t material_offset = scene.materials.size();
        size_t texture_offset = scene.textures.size();

        Scene loaded_scene;
        loaded_scene.from_file(scene.resource_directory + subscene.path, dummy_camera);

        // Add all meshes
        scene.meshes.insert(scene.meshes.end(), loaded_scene.meshes.begin(), loaded_scene.meshes.end());

        for (const SceneInstance& scene_inst : subscene.instances) {
            // Add all point lights
            for (const PointLight& pl : loaded_scene.point_lights) {
                scene.point_lights.push_back(pl);
                scene.point_lights.back().position() = Vec3(scene_inst.transform.matrix * Vec4(pl.position(), 1.0f));
            }

            // Add all directional lights
            for (const DirectionalLight& dl : loaded_scene.directional_lights) {
                scene.directional_lights.push_back(dl);
                scene.directional_lights.back().direction() = Vec3(scene_inst.transform.matrix * Vec4(dl.direction(), 0.0f));
            }
        }

        // Add all object variants and instances from the subscenes and their instances
        for (const ObjectVariant& loaded_variant : loaded_scene.object_variants) {

            scene.object_variants.emplace_back();
            scene.object_variants.back().mesh_index = loaded_variant.mesh_index + mesh_offset;
            scene.object_variants.back().material_index = loaded_variant.material_index + material_offset;

            scene.object_variants.back().instances.resize(loaded_variant.instances.size() * subscene.instances.size());
            size_t inst_idx = 0;
            for (const SceneInstance& scene_inst : subscene.instances) {
                for (const Instance& obj_inst : loaded_variant.instances)
                    scene.object_variants.back().instances[inst_idx++].transform.matrix = scene_inst.transform.matrix * obj_inst.transform.matrix;
            }
            assert(inst_idx == loaded_variant.instances.size() * subscene.instances.size());
        }

        // Add all materials and offset the texture indices
        scene.materials.insert(scene.materials.end(), loaded_scene.materials.begin(), loaded_scene.materials.end());
        for (size_t i = material_offset; i < scene.materials.size(); i++)
            scene.materials[i].offset_maps_by(static_cast<uint32_t>(texture_offset));

        // Add all textures and update their relative path
        scene.textures.reserve(scene.textures.size() + loaded_scene.textures.size());
        for (size_t i = 0; i < loaded_scene.textures.size(); i++)
            scene.textures.push_back(std::move(loaded_scene.textures[i]));
    }

    assert(scene.check_valid());
}