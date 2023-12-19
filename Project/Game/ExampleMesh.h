#pragma once

/* -------------------------------------------------------------------------- *
 * Plane mesh
 * -------------------------------------------------------------------------- */

#define MAX_PLANE_VERTEX_COUNT 1024 * 1024 * 4

struct plane_vertex_t {
	float position[3];
	float normal[3];
	float uv[2];
};

struct plane_mesh_t {
	float width;
	float height;
	uint32_t rows;
	uint32_t columns;
	uint64_t vertex_count;
	uint64_t index_count;
	plane_vertex_t vertices[MAX_PLANE_VERTEX_COUNT];
	uint32_t indices[MAX_PLANE_VERTEX_COUNT * 6];
};

struct plane_mesh_init_options_t {
	float width;
	float height;
	uint32_t rows;
	uint32_t columns;
};

void plane_mesh_init(plane_mesh_t* plane_mesh, plane_mesh_init_options_t* options);

/* -------------------------------------------------------------------------- *
 * Box mesh
 * -------------------------------------------------------------------------- */

#define BOX_MESH_FACES_COUNT 6
#define BOX_MESH_VERTICES_PER_SIDE 4
#define BOX_MESH_INDICES_PER_SIZE 6
#define BOX_MESH_F32S_PER_VERTEX                                               \
  14 // position : vec3f, tangent : vec3f, bitangent : vec3f, normal : vec3f, uv
 // :vec2f
#define BOX_MESH_VERTEX_STRIDE (BOX_MESH_F32S_PER_VERTEX * 4)
#define BOX_MESH_VERTICES_COUNT                                                \
  (BOX_MESH_FACES_COUNT * BOX_MESH_VERTICES_PER_SIDE * BOX_MESH_F32S_PER_VERTEX)
#define BOX_MESH_INDICES_COUNT                                                 \
  (BOX_MESH_FACES_COUNT * BOX_MESH_INDICES_PER_SIZE)

struct box_mesh_t {
	uint64_t vertex_count;
	uint64_t index_count;
	float vertex_array[BOX_MESH_VERTICES_COUNT];
	uint32_t index_array[BOX_MESH_INDICES_COUNT];
	uint32_t vertex_stride;
};

/**
 * @brief Constructs a box mesh with the given dimensions.
 * The vertex buffer will have the following vertex fields (in the given order):
 *   position  : float32x3
 *   normal    : float32x3
 *   uv        : float32x2
 *   tangent   : float32x3
 *   bitangent : float32x3
 * @param width the width of the box
 * @param height the height of the box
 * @param depth the depth of the box
 * @returns the box mesh with tangent and bitangents.
 */
void box_mesh_create_with_tangents(box_mesh_t* box_mesh, float width, float height, float depth);

/* -------------------------------------------------------------------------- *
 * Cube mesh
 * -------------------------------------------------------------------------- */

struct cube_mesh_t {
	uint64_t vertex_size; /* Byte size of one cube vertex. */
	uint64_t position_offset;
	uint64_t color_offset; /* Byte offset of cube vertex color attribute. */
	uint64_t uv_offset;
	uint64_t vertex_count;
	float vertex_array[360];
};

void cube_mesh_init(cube_mesh_t* cube_mesh);

/* -------------------------------------------------------------------------- *
 * Indexed cube mesh
 * -------------------------------------------------------------------------- */

struct indexed_cube_mesh_t {
	uint64_t vertex_count;
	uint64_t index_count;
	uint64_t color_count;
	float vertex_array[3 * 8];
	uint32_t index_array[2 * 3 * 6];
	uint8_t color_array[4 * 8];
};

void indexed_cube_mesh_init(indexed_cube_mesh_t* cube_mesh);

/* -------------------------------------------------------------------------- *
 * Sphere mesh
 * -------------------------------------------------------------------------- */

struct sphere_mesh_t {
	struct {
		float* data;
		uint64_t length;
	} vertices;
	struct {
		uint16_t* data;
		uint64_t length;
	} indices;
};

struct sphere_mesh_layout_t {
	uint32_t vertex_stride;
	uint32_t positions_offset;
	uint32_t normal_offset;
	uint32_t uv_offset;
};

void sphere_mesh_layout_init(sphere_mesh_layout_t* sphere_layout);
void sphere_mesh_init(sphere_mesh_t* sphere_mesh, float radius, uint32_t width_segments, uint32_t height_segments, float randomness);
void sphere_mesh_destroy(sphere_mesh_t* sphere_mesh);
