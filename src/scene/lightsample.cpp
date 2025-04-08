#include "lightsample.h"

// Returns the power of the emitter.
static float build_emitter_table(
    AliasTable<EmitterTriangleData>& table,
    const Mesh& mesh,
    const PbrMaterial& mat)
{
    assert(mesh.get_indexed_triangles().size() > 0);

    std::vector<float> weights;
    std::vector<EmitterTriangleData> values;
    weights.resize(mesh.get_indexed_triangles().size());
    values.resize(mesh.get_indexed_triangles().size());

    for (size_t i = 0; i < mesh.get_indexed_triangles().size(); i++) {
        const IndexedTriangle& tri = mesh.get_indexed_triangles()[i];
        const Vertex& v_a = mesh.get_vertices()[tri.indices[0]];
        const Vertex& v_b = mesh.get_vertices()[tri.indices[1]];
        const Vertex& v_c = mesh.get_vertices()[tri.indices[2]];

        const Vec3 ab = v_b.position - v_a.position;
        const Vec3 ac = v_c.position - v_a.position;

        const float area = glm::length(cross(ab, ac)) * 0.5f;

        weights[i] = area;
        values[i] = { .index_a = tri.indices[0], .index_b = tri.indices[1], .index_c = tri.indices[2] };
    }

    assert(values.size() == weights.size());
    table.build(values.data(), weights.data(), values.size());

    return 1.0f; // TODO
}

TableLightSampler::TableLightSampler(const Scene& scene, const uint32_t* instance_offsets)
{
    std::vector<float> weights;
    std::vector<LightBinData> values;

    size_t table_offset = 0;

    // Create tables for emitters
    for (size_t i = 0; i < scene.get_object_variants().size(); i++) {
        const ObjectVariant& variant = scene.get_object_variants()[i];

        if (!scene.get_materials()[variant.material_index].is_emitter())
            continue;

        assert(variant.instances.size() > 0);
        assert(table.size_in_bytes() % 4 == 0);

        emitter_tables.emplace_back();
        float power = build_emitter_table(
            emitter_tables.back(),
            scene.get_meshes()[variant.mesh_index],
            scene.get_materials()[variant.material_index]);

        weights.push_back(power * variant.instances.size());
        values.emplace_back();
        std::cout << "emitter " << i << " processed, tris " << emitter_tables.back().size() << std::endl;
        values.back().light_type = LIGHT_TYPE_EMITTER;
        values.back().emitter_object.table_offset = table_offset;
        values.back().emitter_object.table_bin_count = emitter_tables.back().size();
        values.back().emitter_object.instance_offset = instance_offsets[i];
        values.back().emitter_object.instance_count = variant.instances.size();

        table_offset += table.size_in_bytes() / granularity;
    }

    size_t bin_count = scene.get_point_lights().size() + scene.get_directional_lights().size() + emitter_tables.size();
    std::cout << "done, bins: " << bin_count << std::endl;

    for (size_t i = 0; i < values.size(); i++)
        values[i].emitter_object.table_offset += table.size_in_bytes(bin_count) / granularity;

    weights.reserve(bin_count);
    values.reserve(bin_count);

    for (const PointLight& point_light : scene.get_point_lights()) {
        weights.push_back(point_light.power());
        values.emplace_back();
        values.back().light_type = LIGHT_TYPE_POINT;
        values.back().infinite_light = { .position_direction = point_light.position(), .emittance = point_light.colour() };
    }

    for (const DirectionalLight& dir_light : scene.get_directional_lights()) {
        weights.push_back(dir_light.power());
        values.emplace_back();
        values.back().light_type = LIGHT_TYPE_DIRECTIONAL;
        values.back().infinite_light = { .position_direction = dir_light.direction(), .emittance = dir_light.colour() };
    }

    assert(values.size() == weights.size());
    table.build(values.data(), weights.data(), values.size());
}

size_t TableLightSampler::size_in_bytes(const Scene& scene)
{
    size_t emitter_count = 0;
    size_t emitter_table_size = 0;
    for (size_t i = 0; i < scene.get_object_variants().size(); i++) {
        const ObjectVariant& variant = scene.get_object_variants()[i];
        if (scene.get_materials()[variant.material_index].is_emitter()) {
            emitter_count += 1;
            emitter_table_size += AliasTable<EmitterTriangleData>::size_in_bytes(scene.get_meshes()[variant.mesh_index].get_indexed_triangles().size());
        }
    }
    size_t bin_count = emitter_count + scene.get_point_lights().size() + scene.get_directional_lights().size();
    return emitter_table_size + AliasTable<LightBinData>::size_in_bytes(bin_count);
}

size_t TableLightSampler::size_in_bytes() const
{
    size_t size = 0;
    size += table.size_in_bytes();
    assert(size % 4 == 0);

    for (const auto& emitter_table : emitter_tables) {
        size += emitter_table.size_in_bytes();
        assert(size % 4 == 0);
    }

    return size;
}

size_t TableLightSampler::copy(void* dest) const
{
    uint8_t* ptr = (uint8_t*)dest;
    size_t size = table.copy(ptr);
    ptr += size;

    for (const auto& emitter_table : emitter_tables) {
        size = emitter_table.copy(ptr);
        ptr += size;
    }

    return ptr - (uint8_t*)dest;
}
