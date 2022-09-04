#include "mesh_utils.hpp"

#include <glm/common.hpp>
#include <glm/geometric.hpp>

#include <sstream>
#include <stdexcept>

std::pair<std::vector<vertex>, std::vector<std::uint32_t>> load_obj(std::istream & input, float scale)
{
	std::vector<vertex> vertices;
	std::vector<std::uint32_t> indices;

	for (std::string line; std::getline(input, line);)
	{
		std::istringstream line_stream(line);

		char type;
		line_stream >> type;

		if (type == '#')
			continue;

		if (type == 'o')
			continue;

		if (type == 's')
			continue;

		if (type == 'v')
		{
			vertex v;
			line_stream >> v.position.x >> v.position.y >> v.position.z;
			v.position *= scale;
			vertices.push_back(v);
			continue;
		}

		if (type == 'f')
		{
			std::uint32_t i0, i1, i2;
			line_stream >> i0 >> i1 >> i2;
			--i0;
			--i1;
			--i2;
			indices.push_back(i0);
			indices.push_back(i1);
			indices.push_back(i2);
			continue;
		}

		throw std::runtime_error("Unknown OBJ row type: " + std::string(1, type));
	}

	return {vertices, indices};
}

std::pair<glm::vec3, glm::vec3> bbox(std::vector<vertex> const & vertices)
{
	static const float inf = std::numeric_limits<float>::infinity();

	glm::vec3 min = glm::vec3( inf);
	glm::vec3 max = glm::vec3(-inf);

	for (auto const & v : vertices)
	{
		min = glm::min(min, v.position);
		max = glm::max(max, v.position);
	}

	return {min, max};
}

void fill_normals(std::vector<vertex> & vertices, std::vector<std::uint32_t> const & indices)
{
	for (auto & v : vertices)
		v.normal = glm::vec3(0.f);

	for (std::size_t i = 0; i < indices.size(); i += 3)
	{
		auto & v0 = vertices[indices[i + 0]];
		auto & v1 = vertices[indices[i + 1]];
		auto & v2 = vertices[indices[i + 2]];

		glm::vec3 n = glm::cross(v1.position - v0.position, v2.position - v0.position);
		v0.normal += n;
		v1.normal += n;
		v2.normal += n;
	}

	for (auto & v : vertices)
		v.normal = glm::normalize(v.normal);
}
