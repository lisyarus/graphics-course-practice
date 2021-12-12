#include "frustum.hpp"

#include <glm/geometric.hpp>

frustum::frustum(glm::mat4 const & view_projection)
{
	glm::mat4 m = glm::inverse(view_projection);
	for (std::size_t i = 0; i < 8; ++i)
	{
		glm::vec4 v;
		v.x = (i & 1) ? 1.f : -1.f;
		v.y = (i & 2) ? 1.f : -1.f;
		v.z = (i & 4) ? 1.f : -1.f;
		v.w = 1.f;

		v = m * v;
		v = v / v.w;
		vertices[i] = v.xyz();
	}

	auto n = [&](std::size_t i0, std::size_t i1, std::size_t i2) -> glm::vec3
	{
		return glm::cross(vertices[i1] - vertices[i0], vertices[i2] - vertices[i0]);
	};

	face_normals = {
		n(0, 1, 2),
		n(4, 0, 2),
		n(1, 5, 3),
		n(0, 4, 1),
		n(2, 3, 6),
	};

	auto e = [&](std::size_t i0, std::size_t i1) -> glm::vec3
	{
		return vertices[i1] - vertices[i0];
	};

	edge_directions = {
		e(0, 1),
		e(0, 2),
		e(0, 4),
		e(1, 5),
		e(2, 6),
		e(3, 7),
	};
}
