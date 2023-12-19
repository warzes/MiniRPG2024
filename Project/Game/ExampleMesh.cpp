#include "Engine.h"
#include "ExampleMesh.h"
#include "Examples.h"

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

/* -------------------------------------------------------------------------- *
* Plane mesh
* -------------------------------------------------------------------------- */

void plane_mesh_generate_vertices(plane_mesh_t* plane_mesh)
{
	plane_mesh->vertex_count = 0;
	const float row_height = plane_mesh->height / (float)plane_mesh->rows;
	const float col_width = plane_mesh->width / (float)plane_mesh->columns;
	float x = 0.0f, y = 0.0f;
	for (uint32_t row = 0; row <= plane_mesh->rows; ++row) {
		y = row * row_height;

		for (uint32_t col = 0; col <= plane_mesh->columns; ++col) {
			x = col * col_width;

			plane_vertex_t* vertex = &plane_mesh->vertices[plane_mesh->vertex_count];
			{
				// Vertex position
				vertex->position[0] = x;
				vertex->position[1] = y;
				vertex->position[2] = 0.0f;

				// Vertex normal
				vertex->normal[0] = 0.0f;
				vertex->normal[1] = 0.0f;
				vertex->normal[2] = 1.0f;

				// Vertex uv
				vertex->uv[0] = col / plane_mesh->columns;
				vertex->uv[1] = 1 - row / plane_mesh->rows;
			}
			++plane_mesh->vertex_count;
		}
	}
}

void plane_mesh_generate_indices(plane_mesh_t* plane_mesh)
{
	plane_mesh->index_count = 0;
	const uint32_t columns_offset = plane_mesh->columns + 1;
	uint32_t left_bottom = 0, right_bottom = 0, left_up = 0, right_up = 0;
	for (uint32_t row = 0; row < plane_mesh->rows; ++row) {
		for (uint32_t col = 0; col < plane_mesh->columns; ++col) {
			left_bottom = columns_offset * row + col;
			right_bottom = columns_offset * row + (col + 1);
			left_up = columns_offset * (row + 1) + col;
			right_up = columns_offset * (row + 1) + (col + 1);

			// CCW frontface
			plane_mesh->indices[plane_mesh->index_count++] = left_up;
			plane_mesh->indices[plane_mesh->index_count++] = left_bottom;
			plane_mesh->indices[plane_mesh->index_count++] = right_bottom;

			plane_mesh->indices[plane_mesh->index_count++] = right_up;
			plane_mesh->indices[plane_mesh->index_count++] = left_up;
			plane_mesh->indices[plane_mesh->index_count++] = right_bottom;
		}
	}
}

void plane_mesh_init(plane_mesh_t* plane_mesh,
	plane_mesh_init_options_t* options)
{
	// Initialize dimensions
	plane_mesh->width = options ? options->width : 1.0f;
	plane_mesh->height = options ? options->height : 1.0f;
	plane_mesh->rows = options ? options->rows : 1;
	plane_mesh->columns = options ? options->columns : 1;

	assert((plane_mesh->rows + 1) * (plane_mesh->columns + 1) < MAX_PLANE_VERTEX_COUNT);

	// Generate vertices and indices
	plane_mesh_generate_vertices(plane_mesh);
	plane_mesh_generate_indices(plane_mesh);
}

/* -------------------------------------------------------------------------- *
* Box mesh
* -------------------------------------------------------------------------- */

void box_mesh_create_with_tangents(box_mesh_t* box_mesh, float width, float height, float depth)
{
	//    __________
	//   /         /|      y
	//  /   +y    / |      ^
	// /_________/  |      |
	// |         |+x|      +---> x
	// |   +z    |  |     /
	// |         | /     z
	// |_________|/
	//
	const uint8_t p_x = 0; /* +x */
	const uint8_t n_x = 1; /* -x */
	const uint8_t p_y = 2; /* +y */
	const uint8_t n_y = 3; /* -y */
	const uint8_t p_z = 4; /* +z */
	const uint8_t n_z = 5; /* -z */

	struct {
		uint8_t tangent;
		uint8_t bitangent;
		uint8_t normal;
	} faces[BOX_MESH_FACES_COUNT] = {
		{.tangent = n_z, .bitangent = p_y, .normal = p_x, },
		{.tangent = p_z, .bitangent = p_y, .normal = n_x, },
		{.tangent = p_x, .bitangent = n_z, .normal = p_y, },
		{.tangent = p_x, .bitangent = p_z, .normal = n_y, },
		{.tangent = p_x, .bitangent = p_y, .normal = p_z, },
		{.tangent = n_x, .bitangent = p_y, .normal = n_z, },
	};

	uint32_t vertices_per_side = BOX_MESH_VERTICES_PER_SIDE;
	box_mesh->vertex_count = BOX_MESH_VERTICES_COUNT;
	box_mesh->index_count = BOX_MESH_INDICES_COUNT;
	box_mesh->vertex_stride = BOX_MESH_VERTEX_STRIDE;

	const float half_vecs[BOX_MESH_FACES_COUNT][3] = {
		{+width / 2.0f, 0.0f, 0.0f},  /* +x */
		{-width / 2.0f, 0.0f, 0.0f},  /* -x */
		{0.0f, +height / 2.0f, 0.0f}, /* +y */
		{0.0f, -height / 2.0f, 0.0f}, /* -y */
		{0.0f, 0.0f, +depth / 2.0f},  /* +z */
		{0.0f, 0.0f, -depth / 2.0f},  /* -z */
	};

	uint32_t vertex_offset = 0;
	uint32_t index_offset = 0;
	for (uint8_t face_index = 0; face_index < BOX_MESH_FACES_COUNT/*ARRAY_SIZE(faces)*/; ++face_index)
	{
		const float* tangent = half_vecs[faces[face_index].tangent];
		const float* bitangent = half_vecs[faces[face_index].bitangent];
		const float* normal = half_vecs[faces[face_index].normal];

		for (uint8_t u = 0; u < 2; ++u) {
			for (uint8_t v = 0; v < 2; ++v) {
				for (uint8_t i = 0; i < 3; ++i) {
					box_mesh->vertex_array[vertex_offset++]
						= normal[i] + (u == 0 ? -1 : 1) * tangent[i]
						+ (v == 0 ? -1 : 1) * bitangent[i];
				}
				for (uint8_t i = 0; i < 3; i++) {
					box_mesh->vertex_array[vertex_offset++] = normal[i];
				}
				box_mesh->vertex_array[vertex_offset++] = u;
				box_mesh->vertex_array[vertex_offset++] = v;
				for (uint8_t i = 0; i < 3; ++i) {
					box_mesh->vertex_array[vertex_offset++] = tangent[i];
				}
				for (uint8_t i = 0; i < 3; i++) {
					box_mesh->vertex_array[vertex_offset++] = bitangent[i];
				}
			}
		}

		box_mesh->index_array[index_offset++] = face_index * vertices_per_side + 0;
		box_mesh->index_array[index_offset++] = face_index * vertices_per_side + 2;
		box_mesh->index_array[index_offset++] = face_index * vertices_per_side + 1;

		box_mesh->index_array[index_offset++] = face_index * vertices_per_side + 2;
		box_mesh->index_array[index_offset++] = face_index * vertices_per_side + 3;
		box_mesh->index_array[index_offset++] = face_index * vertices_per_side + 1;
	}
}

/* -------------------------------------------------------------------------- *
* Cube mesh
* -------------------------------------------------------------------------- */

void cube_mesh_init(cube_mesh_t* cube_mesh)
{
	(*cube_mesh) = {
	.vertex_size = 4 * 10, // Byte size of one cube vertex.
	.position_offset = 0,
	.color_offset = 4 * 4, // Byte offset of cube vertex color attribute.
	.uv_offset = 4 * 8,
	.vertex_count = 36,
	.vertex_array = {
			// float4 position, float4 color, float2 uv,
			1, -1, 1, 1,   1, 0, 1, 1,  0, 1, //
			-1, -1, 1, 1,  0, 0, 1, 1,  1, 1, //
			-1, -1, -1, 1, 0, 0, 0, 1,  1, 0, //
			1, -1, -1, 1,  1, 0, 0, 1,  0, 0, //
			1, -1, 1, 1,   1, 0, 1, 1,  0, 1, //
			-1, -1, -1, 1, 0, 0, 0, 1,  1, 0, //

			1, 1, 1, 1,    1, 1, 1, 1,  0, 1, //
			1, -1, 1, 1,   1, 0, 1, 1,  1, 1, //
			1, -1, -1, 1,  1, 0, 0, 1,  1, 0, //
			1, 1, -1, 1,   1, 1, 0, 1,  0, 0, //
			1, 1, 1, 1,    1, 1, 1, 1,  0, 1, //
			1, -1, -1, 1,  1, 0, 0, 1,  1, 0, //

			-1, 1, 1, 1,   0, 1, 1, 1,  0, 1, //
			1, 1, 1, 1,    1, 1, 1, 1,  1, 1, //
			1, 1, -1, 1,   1, 1, 0, 1,  1, 0, //
			-1, 1, -1, 1,  0, 1, 0, 1,  0, 0, //
			-1, 1, 1, 1,   0, 1, 1, 1,  0, 1, //
			1, 1, -1, 1,   1, 1, 0, 1,  1, 0, //

			-1, -1, 1, 1,  0, 0, 1, 1,  0, 1, //
			-1, 1, 1, 1,   0, 1, 1, 1,  1, 1, //
			-1, 1, -1, 1,  0, 1, 0, 1,  1, 0, //
			-1, -1, -1, 1, 0, 0, 0, 1,  0, 0, //
			-1, -1, 1, 1,  0, 0, 1, 1,  0, 1, //
			-1, 1, -1, 1,  0, 1, 0, 1,  1, 0, //

			1, 1, 1, 1,    1, 1, 1, 1,  0, 1, //
			-1, 1, 1, 1,   0, 1, 1, 1,  1, 1, //
			-1, -1, 1, 1,  0, 0, 1, 1,  1, 0, //
			-1, -1, 1, 1,  0, 0, 1, 1,  1, 0, //
			1, -1, 1, 1,   1, 0, 1, 1,  0, 0, //
			1, 1, 1, 1,    1, 1, 1, 1,  0, 1, //

			1, -1, -1, 1,  1, 0, 0, 1,  0, 1, //
			-1, -1, -1, 1, 0, 0, 0, 1,  1, 1, //
			-1, 1, -1, 1,  0, 1, 0, 1,  1, 0, //
			1, 1, -1, 1,   1, 1, 0, 1,  0, 0, //
			1, -1, -1, 1,  1, 0, 0, 1,  0, 1, //
			-1, 1, -1, 1,  0, 1, 0, 1,  1, 0, //
			},
	};
}

/* -------------------------------------------------------------------------- *
* Indexed cube mesh
* -------------------------------------------------------------------------- */

void indexed_cube_mesh_init(indexed_cube_mesh_t* cube_mesh)
{
	(*cube_mesh) = {
	.vertex_count = 8,
	.index_count = 2 * 3 * 6,
	.color_count = 8,
	.vertex_array = {
	-1.0f, -1.0f, -1.0f, // 0
	1.0f, -1.0f, -1.0f, // 1
	1.0f, -1.0f,  1.0f, // 2
	-1.0f, -1.0f,  1.0f, // 3
	-1.0f,  1.0f, -1.0f, // 4
	1.0f,  1.0f, -1.0f, // 5
	1.0f,  1.0f,  1.0f, // 6
	-1.0f,  1.0f,  1.0f, // 7
	},
	.index_array = {
			// BOTTOM
			0, 1, 2, /* */  0, 2, 3,
			// TOP
			4, 5, 6,  /* */  4, 6, 7,
			// FRONT
			3, 2, 6,  /* */  3, 6, 7,
			// BACK
			1, 0, 4,  /* */  1, 4, 5,
			// LEFT
			3, 0, 7,  /* */  0, 7, 4,
			// RIGHT
			2, 1, 6,  /* */  1, 6, 5,
			},
	};

	float* vertices = cube_mesh->vertex_array;
	uint8_t* colors = cube_mesh->color_array;
	float x = 0.0f, y = 0.0f, z = 0.0f;
	for (uint8_t i = 0; i < 8; ++i) {
		x = vertices[3 * i + 0];
		y = vertices[3 * i + 1];
		z = vertices[3 * i + 2];

		colors[4 * i + 0] = 255 * (x + 1) / 2;
		colors[4 * i + 1] = 255 * (y + 1) / 2;
		colors[4 * i + 2] = 255 * (z + 1) / 2;
		colors[4 * i + 3] = 255;
	}
}

/* -------------------------------------------------------------------------- *
* Sphere mesh
* -------------------------------------------------------------------------- */

void sphere_mesh_layout_init(sphere_mesh_layout_t* sphere_layout)
{
	sphere_layout->vertex_stride = 8 * 4;
	sphere_layout->positions_offset = 0;
	sphere_layout->normal_offset = 3 * 4;
	sphere_layout->uv_offset = 6 * 4;
}

// Borrowed and simplified from:
// https://github.com/mrdoob/three.js/blob/master/src/geometries/SphereGeometry.js
void sphere_mesh_init(sphere_mesh_t* sphere_mesh, float radius,
	uint32_t width_segments, uint32_t height_segments,
	float randomness)
{
	width_segments = MAX(3u, floor(width_segments));
	height_segments = MAX(2u, floor(height_segments));

	uint32_t vertices_count
		= (width_segments + 1) * (height_segments + 1) * (3 + 3 + 2);
	float* vertices = (float*)malloc(vertices_count * sizeof(float));

	uint32_t indices_count = width_segments * height_segments * 6;
	uint16_t* indices = (uint16_t*)malloc(indices_count * sizeof(uint16_t));

	glm::vec3 first_vertex = glm::vec3(0.0f);
	glm::vec3 vertex = glm::vec3(0.0f);
	glm::vec3 normal = glm::vec3(0.0f);

	uint32_t index = 0;
	uint32_t grid_count = height_segments + 1;
	uint16_t** grid = (uint16_t**)malloc(grid_count * sizeof(uint16_t*));

	/* Generate vertices, normals and uvs */
	uint32_t vc = 0, ic = 0, gc = 0;
	for (uint32_t iy = 0; iy <= height_segments; ++iy) {
		uint16_t* vertices_row
			= (uint16_t*)malloc((width_segments + 1) * sizeof(uint16_t));
		uint32_t vri = 0;
		const float v = iy / (float)height_segments;

		// special case for the poles
		float u_offset = 0.0f;
		if (iy == 0) {
			u_offset = 0.5f / width_segments;
		}
		else if (iy == height_segments) {
			u_offset = -0.5f / width_segments;
		}

		for (uint32_t ix = 0; ix <= width_segments; ++ix) {
			const float u = ix / (float)width_segments;

			/* Poles should just use the same position all the way around. */
			if (ix == width_segments) {
				vertex = first_vertex;
			}
			else if (ix == 0 || (iy != 0 && iy != height_segments)) {
				const float rr = radius + (random_float() - 0.5f) * 2.0f * randomness * radius;

				/* vertex */
				vertex[0] = -rr * cos(u * glm::pi<float>() * 2.0f) * sin(v * glm::pi<float>());
				vertex[1] = rr * cos(v * glm::pi<float>());
				vertex[2] = rr * sin(u * glm::pi<float>() * 2.0f) * sin(v * glm::pi<float>());

				if (ix == 0) {
					first_vertex = vertex;
				}
			}
			vertices[vc++] = vertex[0];
			vertices[vc++] = vertex[1];
			vertices[vc++] = vertex[2];

			/* normal */
			normal = vertex;
			normal = glm::normalize(normal);
			vertices[vc++] = normal[0];
			vertices[vc++] = normal[1];
			vertices[vc++] = normal[2];

			/* uv */
			vertices[vc++] = u + u_offset;
			vertices[vc++] = 1 - v;
			vertices_row[vri++] = index++;
		}

		grid[gc++] = vertices_row;
	}

	/* indices */
	uint16_t a = 0, b = 0, c = 0, d = 0;
	for (uint32_t iy = 0; iy < height_segments; ++iy) {
		for (uint32_t ix = 0; ix < width_segments; ++ix) {
			a = grid[iy][ix + 1];
			b = grid[iy][ix];
			c = grid[iy + 1][ix];
			d = grid[iy + 1][ix + 1];

			if (iy != 0) {
				indices[ic++] = a;
				indices[ic++] = b;
				indices[ic++] = d;
			}
			if (iy != height_segments - 1) {
				indices[ic++] = b;
				indices[ic++] = c;
				indices[ic++] = d;
			}
		}
	}

	/* Cleanup temporary grid */
	for (uint32_t gci = 0; gci < grid_count; ++gci) {
		free(grid[gci]);
	}
	free(grid);

	/* Sphere */
	memset(sphere_mesh, 0, sizeof(*sphere_mesh));
	sphere_mesh->vertices.data = vertices;
	sphere_mesh->vertices.length = vc;
	sphere_mesh->indices.data = indices;
	sphere_mesh->indices.length = ic;
}

void sphere_mesh_destroy(sphere_mesh_t* sphere_mesh)
{
	if (sphere_mesh->vertices.data) {
		free(sphere_mesh->vertices.data);
	}
	if (sphere_mesh->indices.data) {
		free(sphere_mesh->indices.data);
	}
	memset(sphere_mesh, 0, sizeof(*sphere_mesh));
}