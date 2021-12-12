#pragma once

#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>

#include <array>

struct frustum
{
	std::array<glm::vec3, 8> vertices;
	std::array<glm::vec3, 5> face_normals;
	std::array<glm::vec3, 6> edge_directions;

	frustum(glm::mat4 const & view_projection);
};
