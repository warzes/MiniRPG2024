#pragma once

#include <tiny_obj_loader.h>

struct VertexAttributes 
{
	glm::vec3 position;
	glm::vec3 tangent; // T = local X axis
	glm::vec3 bitangent; // B = local Y axis
	glm::vec3 normal; // N = local Z axis

	glm::vec3 color;
	glm::vec2 uv;
};

glm::mat3x3 computeTBN(const VertexAttributes corners[3], const glm::vec3& expectedN) 
{
	// What we call e in the figure
	glm::vec3 ePos1 = corners[1].position - corners[0].position;
	glm::vec3 ePos2 = corners[2].position - corners[0].position;

	// What we call \bar e in the figure
	glm::vec2 eUV1 = corners[1].uv - corners[0].uv;
	glm::vec2 eUV2 = corners[2].uv - corners[0].uv;

	glm::vec3 T = glm::normalize(ePos1 * eUV2.y - ePos2 * eUV1.y);
	glm::vec3 B = glm::normalize(ePos2 * eUV1.x - ePos1 * eUV2.x);
	glm::vec3 N = glm::cross(T, B);

	// Fix overall orientation
	if (glm::dot(N, expectedN) < 0.0) 
	{
		T = -T;
		B = -B;
		N = -N;
	}

	// Ortho-normalize the (T, B, expectedN) frame
	// a. "Remove" the part of T that is along expected N
	N = expectedN;
	T = glm::normalize(T - glm::dot(T, N) * N);
	// b. Recompute B from N and T
	B = glm::cross(N, T);

	return glm::mat3x3(T, B, N);
}

void populateTextureFrameAttributes(std::vector<VertexAttributes>& vertexData)
{
	size_t triangleCount = vertexData.size() / 3;
	// We compute the local texture frame per triangle
	for (int t = 0; t < triangleCount; ++t)
	{
		VertexAttributes* v = &vertexData[3 * t];

		for (int k = 0; k < 3; ++k)
		{
			glm::mat3x3 TBN = computeTBN(v, v[k].normal);
			v[k].tangent = TBN[0];
			v[k].bitangent = TBN[1];
		}
	}
}

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

			vertexData[offset + i].uv = 
			{
				attrib.texcoords[2 * idx.texcoord_index + 0],
				1 - attrib.texcoords[2 * idx.texcoord_index + 1]
			};
		}
	}

	populateTextureFrameAttributes(vertexData);

	return true;
}