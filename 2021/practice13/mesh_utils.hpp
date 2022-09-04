#pragma once

#include <glm/vec3.hpp>

#include <utility>
#include <vector>
#include <iostream>

struct vertex
{
	glm::vec3 position;
	glm::vec3 normal;
};

std::pair<std::vector<vertex>, std::vector<std::uint32_t>> load_obj(std::istream & input, float scale = 1.f);

std::pair<glm::vec3, glm::vec3> bbox(std::vector<vertex> const & vertices);

void fill_normals(std::vector<vertex> & vertices, std::vector<std::uint32_t> const & indices);
