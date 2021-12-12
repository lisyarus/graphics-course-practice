#pragma once

#include <glm/vec3.hpp>

#include <array>

struct aabb
{
	aabb(glm::vec3 const & min, glm::vec3 const & max);

	std::array<glm::vec3, 8> vertices;
	static const std::array<glm::vec3, 3> face_normals;
	static const std::array<glm::vec3, 3> edge_directions;
};
