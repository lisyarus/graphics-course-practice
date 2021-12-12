#pragma once

#include <glm/vec3.hpp>
#include <glm/geometric.hpp>

#include <limits>
#include <utility>
#include <cmath>

template <typename Body>
std::pair<float, float> project(Body const & b, glm::vec3 const & n)
{
	static constexpr float inf = std::numeric_limits<float>::infinity();

	float min = inf;
	float max = -inf;

	for (auto const & p : b.vertices)
	{
		float v = glm::dot(p, n);
		min = std::min(min, v);
		max = std::max(max, v);
	}

	return {min, max};
}

template <typename Body1, typename Body2>
bool intersect_along(Body1 const & b1, Body2 const & b2, glm::vec3 const & n)
{
	auto [min1, max1] = project(b1, n);
	auto [min2, max2] = project(b2, n);

	return (min1 <= max2) && (min2 <= max1);
}

template <typename Body1, typename Body2>
bool intersect(Body1 const & b1, Body2 const & b2)
{
	for (auto const & n : b1.face_normals)
	{
		if (!intersect_along(b1, b2, n))
			return false;
	}

	for (auto const & n : b2.face_normals)
	{
		if (!intersect_along(b1, b2, n))
			return false;
	}

	for (auto const & e1 : b1.edge_directions)
	{
		for (auto const & e2 : b2.edge_directions)
		{
			glm::vec3 n = glm::cross(e1, e2);
			if (!intersect_along(b1, b2, n))
				return false;
		}
	}

	return true;
}
