#pragma once

#include <tiny_obj_loader.h>

struct VertexAttributes 
{
	glm::vec3 position;
	glm::vec3 normal;
	glm::vec3 color;
};

bool loadGeometryFromObj(const std::filesystem::path& path,  std::vector<VertexAttributes>& vertexData) 
{
	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;

	std::string warn;
	std::string err;

	bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, path.string().c_str());

	if (!warn.empty())
		Warning(warn);

	if (!err.empty())
		Error(err);

	if (!ret)
		return false;

	const auto& shape = shapes[0]; // look at the first shape only

	vertexData.clear();
	for (const auto& shape : shapes)
	{
		size_t offset = vertexData.size();
		vertexData.resize(offset + shape.mesh.indices.size());
		for (size_t i = 0; i < shape.mesh.indices.size(); ++i)
		{
			const tinyobj::index_t& idx = shape.mesh.indices[i];

			vertexData[offset + i].position =
			{
#if 0
				attrib.vertices[3 * idx.vertex_index + 0],
				attrib.vertices[3 * idx.vertex_index + 1],
				attrib.vertices[3 * idx.vertex_index + 2]
#else // соглашение о вертикальной оси
				attrib.vertices[3 * idx.vertex_index + 0],
				-attrib.vertices[3 * idx.vertex_index + 2], // Add a minus to avoid mirroring
				attrib.vertices[3 * idx.vertex_index + 1]
#endif
			};

			vertexData[offset + i].normal =
			{
#if 0
				attrib.normals[3 * idx.normal_index + 0],
				attrib.normals[3 * idx.normal_index + 1],
				attrib.normals[3 * idx.normal_index + 2]
#else // соглашение о вертикальной оси
				attrib.normals[3 * idx.normal_index + 0],
				-attrib.normals[3 * idx.normal_index + 2],
				attrib.normals[3 * idx.normal_index + 1]
#endif
			};

			vertexData[offset + i].color =
			{
				attrib.colors[3 * idx.vertex_index + 0],
				attrib.colors[3 * idx.vertex_index + 1],
				attrib.colors[3 * idx.vertex_index + 2]
			};
		}
	}

	return true;
}