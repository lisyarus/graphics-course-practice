#include "gltf_loader.hpp"

#include <rapidjson/document.h>
#include <rapidjson/istreamwrapper.h>

#include <fstream>
#include <stdexcept>

static unsigned int attribute_type_to_size(std::string const & type)
{
    if (type == "SCALAR") return 1;
    if (type == "VEC2") return 2;
    if (type == "VEC3") return 3;
    if (type == "VEC4") return 4;
    return 0;
}

gltf_model load_gltf(std::filesystem::path const & path)
{
    rapidjson::Document document;

    {
        std::ifstream input(path, std::ios::binary);
        rapidjson::IStreamWrapper stream(input);
        document.ParseStream(stream);
    }

    gltf_model result;

    {
        auto buffers = document["buffers"].GetArray();
        assert(buffers.Size() == 1);

        auto const buffer_uri = buffers[0]["uri"].GetString();

        auto const buffer_path = path.parent_path() / buffer_uri;

        result.buffer.resize(std::filesystem::file_size(buffer_path));
        std::ifstream buffer(buffer_path, std::ios::binary);
        buffer.read(result.buffer.data(), result.buffer.size());
    }

    auto parse_buffer_view = [&](int index) -> gltf_model::buffer_view
    {
        auto view = document["bufferViews"].GetArray()[index].GetObject();
        return {view["byteOffset"].GetUint(), view["byteLength"].GetUint()};
    };

    auto parse_accessor = [&](int index) -> gltf_model::accessor
    {
        auto accessor = document["accessors"].GetArray()[index].GetObject();
        return {
            parse_buffer_view(accessor["bufferView"].GetInt()),
            accessor["componentType"].GetUint(),
            attribute_type_to_size(accessor["type"].GetString()),
            accessor["count"].GetUint(),
        };
    };

    auto parse_texture = [&](int index) -> std::string
    {
        auto const source_index = document["textures"].GetArray()[index]["source"].GetInt();
        return document["images"].GetArray()[source_index]["uri"].GetString();
    };

    auto parse_color = [&](auto const & array)
    {
        return glm::vec4{
            array[0].GetFloat(),
            array[1].GetFloat(),
            array[2].GetFloat(),
            array[3].GetFloat(),
        };
    };

    auto parse_vector = [&](auto const & array)
    {
        return glm::vec3{
            array[0].GetFloat(),
            array[1].GetFloat(),
            array[2].GetFloat(),
        };
    };

    auto parse_bounds = [&](int index)
    {
        auto accessor = document["accessors"].GetArray()[index].GetObject();
        return std::make_pair(
            parse_vector(accessor["min"]),
            parse_vector(accessor["max"])
        );
    };

    for (auto const & mesh : document["meshes"].GetArray())
    {
        auto & result_mesh = result.meshes.emplace_back();
        result_mesh.name = mesh["name"].GetString();

        auto primitives = mesh["primitives"].GetArray();
        assert(primitives.Size() == 1);

        auto const & attributes = primitives[0]["attributes"];

        result_mesh.indices = parse_accessor(primitives[0]["indices"].GetInt());
        result_mesh.position = parse_accessor(attributes["POSITION"].GetInt());
        result_mesh.normal = parse_accessor(attributes["NORMAL"].GetInt());
        result_mesh.texcoord = parse_accessor(attributes["TEXCOORD_0"].GetInt());

        std::tie(result_mesh.min, result_mesh.max) = parse_bounds(attributes["POSITION"].GetInt());

        auto const & material = document["materials"].GetArray()[primitives[0]["material"].GetInt()];

        result_mesh.material.two_sided = material.HasMember("doubleSided") && material["doubleSided"].GetBool();
        result_mesh.material.transparent = material.HasMember("alphaMode") && (material["alphaMode"].GetString() == std::string("BLEND"));

        auto const & pbr = material["pbrMetallicRoughness"];
        if (pbr.HasMember("baseColorTexture"))
            result_mesh.material.texture_path = parse_texture(pbr["baseColorTexture"]["index"].GetInt());
        else if (pbr.HasMember("baseColorFactor"))
            result_mesh.material.color = parse_color(pbr["baseColorFactor"].GetArray());
    }

    return result;
}
