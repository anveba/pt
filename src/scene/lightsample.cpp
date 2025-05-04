#include "lightsample.h"
#include "rng.h"

// Returns the total area of the emitter.
static double build_emitter_table(
    AliasTable<EmitterTriangleData>& table,
    const Mesh& mesh,
    const PbrMaterial& mat)
{
    assert(mesh.get_indexed_triangles().size() > 0);

    std::vector<float> weights;
    std::vector<EmitterTriangleData> values;
    weights.resize(mesh.get_indexed_triangles().size());
    values.resize(mesh.get_indexed_triangles().size());

    double double_total_area = 0.0f;

    for (size_t i = 0; i < mesh.get_indexed_triangles().size(); i++) {
        const IndexedTriangle& tri = mesh.get_indexed_triangles()[i];
        const Vertex& v_a = mesh.get_vertices()[tri.indices[0]];
        const Vertex& v_b = mesh.get_vertices()[tri.indices[1]];
        const Vertex& v_c = mesh.get_vertices()[tri.indices[2]];

        const Vec3 ab = v_b.position - v_a.position;
        const Vec3 ac = v_c.position - v_a.position;

        const float double_area = glm::length(cross(ab, ac));
        double_total_area += double_area;

        weights[i] = static_cast<double>(double_area);
        values[i] = { .index_a = tri.indices[0], .index_b = tri.indices[1], .index_c = tri.indices[2] };
    }

    assert(values.size() == weights.size());
    table.build(values.data(), weights.data(), values.size());

    return double_total_area * 0.5;
}

static double calculate_object_intensity(const Mesh& mesh, const PbrMaterial& material, const std::vector<TextureData>& textures)
{
    constexpr size_t samples_per_triangle = 20; // TODO: use an appropriate value

    Splitmix32 seeder(0xbd7a0d48);
    Xshiro128 rng(seeder.next(), seeder.next(), seeder.next(), seeder.next());

    if ((material.map_bits() & EMISSION_MAP_BIT)) {

        const TextureData& texture_data = textures[material.emission_map_index()];
        double total_power = 0.0;

        // Perform Monte Carlo integration of emission.
        for (size_t i = 0; i < mesh.get_indexed_triangles().size(); i++) {

            const IndexedTriangle& tri = mesh.get_indexed_triangles()[i];
            const Vertex& v_a = mesh.get_vertices()[tri.indices[0]];
            const Vertex& v_b = mesh.get_vertices()[tri.indices[1]];
            const Vertex& v_c = mesh.get_vertices()[tri.indices[2]];

            double triangle_emission_sum = 0.0;

            for (size_t j = 0; j < samples_per_triangle; j++) {

                float beta = 1.0f - std::sqrt(rng.next_float());
                float gamma = (1.0f - beta) * rng.next_float();
                float alpha = 1.0f - beta - gamma;

                Vec2 uv = alpha * v_a.uv + beta * v_b.uv + gamma * v_c.uv;
                float integral;
                uint32_t x = static_cast<uint32_t>(std::roundf(std::modf(uv.x, &integral) * texture_data.get_width()));
                x = std::min(x, texture_data.get_width() - 1);
                uint32_t y = static_cast<uint32_t>(std::roundf(std::modf(uv.y, &integral) * texture_data.get_height()));
                y = std::min(y, texture_data.get_height() - 1);

                Vec4 emission = texture_data.colour_at(x, y);
                triangle_emission_sum += emission.r + emission.g + emission.b;
            }
            total_power += triangle_emission_sum;
        }

        return total_power * material.emission.a / (mesh.get_indexed_triangles().size() * samples_per_triangle);

    } else {
        return (material.emission.r + material.emission.g + material.emission.b) * material.emission.a;
    }
}

TableLightSampler::TableLightSampler(const Scene& scene, const uint32_t* instance_offsets)
{
    struct EmitterTableMetadata
    {
        size_t table_index;
        size_t table_offset;
        double total_mesh_area;
    };

    std::vector<float> weights;
    std::vector<LightBinData> values;
    std::unordered_map<uint32_t, EmitterTableMetadata> mesh_table_map;

    size_t table_offset_accumulation = 0;

    // Create tables for emitters
    for (size_t i = 0; i < scene.get_object_variants().size(); i++) {
        const ObjectVariant& variant = scene.get_object_variants()[i];

        if (!scene.get_materials()[variant.material_index].is_emitter())
            continue;

        assert(variant.instances.size() > 0);

        EmitterTableMetadata table_data;
        if (mesh_table_map.count(variant.mesh_index) == 0) {

            table_data.table_index = emitter_tables.size();

            emitter_tables.emplace_back();
            table_data.total_mesh_area = build_emitter_table(
                emitter_tables.back(),
                scene.get_meshes()[variant.mesh_index],
                scene.get_materials()[variant.material_index]);
            table_data.table_offset = table_offset_accumulation;

            mesh_table_map[variant.mesh_index] = table_data;
            table_offset_accumulation += table.size_in_bytes() / granularity;

        } else {
            table_data = mesh_table_map[variant.mesh_index];
        }

        float intensity = calculate_object_intensity(scene.get_meshes()[variant.mesh_index], scene.get_materials()[variant.material_index], scene.get_textures());
        weights.push_back(intensity);

        #ifdef PT_VERBOSE
        std::cout << "Emitter " << values.size() << " processed"
                  << ", triangles: " << emitter_tables.back().size()
                  << ", area: " << table_data.total_mesh_area
                  << ", intensity: " << intensity << std::endl;
        #endif

        values.emplace_back();
        values.back().light_type = LIGHT_TYPE_EMITTER;
        values.back().emitter_object.table_offset = table_data.table_offset;
        values.back().emitter_object.table_bin_count = emitter_tables[table_data.table_index].size();
        values.back().emitter_object.instance_offset = instance_offsets[i];
        values.back().emitter_object.instance_count = variant.instances.size();
    }

    size_t bin_count = scene.get_point_lights().size() + scene.get_directional_lights().size() + emitter_tables.size();
    
    #ifdef PT_VERBOSE
    std::cout << "Alias table for lights has " << bin_count << " bins." << std::endl;
    #endif

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
