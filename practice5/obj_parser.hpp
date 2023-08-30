#pragma once

#include <vector>
#include <filesystem>

struct obj_data
{
    struct vertex
    {
        std::array<float, 3> position;
        std::array<float, 3> normal;
        std::array<float, 2> texcoord;
    };

    std::vector<vertex> vertices;
    std::vector<std::uint32_t> indices;
};

obj_data parse_obj(std::filesystem::path const & path);
