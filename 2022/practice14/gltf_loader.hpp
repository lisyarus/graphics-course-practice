#pragma once

#include <filesystem>
#include <vector>
#include <string>
#include <optional>
#include <unordered_map>
#include <algorithm>

#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/compatibility.hpp>

struct gltf_model
{
    struct buffer_view
    {
        unsigned int offset;
        unsigned int size;
    };

    struct accessor
    {
        buffer_view view;
        unsigned int type;
        unsigned int size;
        unsigned int count;
    };

    struct material
    {
        bool two_sided;
        bool transparent;
        std::optional<std::string> texture_path;
        std::optional<glm::vec4> color;
    };

    struct mesh
    {
        std::string name;
        struct material material;

        accessor indices;

        accessor position;
        accessor normal;
        accessor texcoord;

        glm::vec3 min;
        glm::vec3 max;
    };

    std::vector<char> buffer;
    std::vector<mesh> meshes;
};

gltf_model load_gltf(std::filesystem::path const & path);
