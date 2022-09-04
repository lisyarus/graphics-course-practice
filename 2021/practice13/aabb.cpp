#include "aabb.hpp"

aabb::aabb(glm::vec3 const & min, glm::vec3 const & max)
{
	for (std::size_t i = 0; i < 8; ++i)
	{
		vertices[i].x = (i & 1) ? max.x : min.x;
		vertices[i].y = (i & 2) ? max.y : min.y;
		vertices[i].z = (i & 4) ? max.z : min.z;
	}
}

const std::array<glm::vec3, 3> aabb::face_normals =
{
	glm::vec3(1.f, 0.f, 0.f),
	glm::vec3(0.f, 1.f, 0.f),
	glm::vec3(0.f, 0.f, 1.f),
};

const std::array<glm::vec3, 3> aabb::edge_directions =
{
	glm::vec3(1.f, 0.f, 0.f),
	glm::vec3(0.f, 1.f, 0.f),
	glm::vec3(0.f, 0.f, 1.f),
};
