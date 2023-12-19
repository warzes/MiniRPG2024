#pragma once

enum camera_type_enum 
{
	CameraType_LookAt = 0,
	CameraType_FirstPerson = 1
};

struct camera_t
{
	glm::vec3 rotation = glm::vec3(0.0f);
	glm::vec3 position = glm::vec3(0.0f);
	glm::vec4 view_pos = glm::vec4(0.0f);
	camera_type_enum type = CameraType_LookAt;
	float fov = 0.0f;
	float znear = 0.0f;
	float zfar = 0.0f;
	float rotation_speed = 1.0f;
	float movement_speed = 1.0f;
	bool updated = false;
	bool flip_y = false;
	struct {
		glm::mat4 perspective = glm::mat4(1.0f);
		glm::mat4 view = glm::mat4(1.0f);
	} matrices;
	struct {
		bool left = false;
		bool right = false;
		bool up = false;
		bool down = false;
	} keys;

	void SetPosition(const glm::vec3& position);
	void SetRotation(const glm::vec3& rotate);
	void SetPerspective(float fov, float aspect, float znear, float zfar);

	void UpdateViewMatrix();
};

inline void camera_t::SetPosition(const glm::vec3& pos)
{
	position = pos;
	UpdateViewMatrix();
}

inline void camera_t::SetRotation(const glm::vec3& rotate)
{
	rotation = rotate;
	UpdateViewMatrix();
}

inline void camera_t::SetPerspective(float fov, float aspect, float znear, float zfar)
{
	this->fov = fov;
	this->znear = znear;
	this->zfar = zfar;

	matrices.perspective = glm::perspective(glm::radians(fov), aspect, znear, zfar);
	if (flip_y)
		matrices.perspective[1][1] *= -1.0f;
}

inline void camera_t::UpdateViewMatrix()
{
	glm::mat4 rot_mat = glm::mat4(1.0f);
	glm::mat4 trans_mat = glm::mat4(1.0f);

	rot_mat = glm::rotate(rot_mat, glm::radians(rotation[0] * (flip_y ? -1.0f : 1.0f)), { 1.0f, 0.0f, 0.0f });
	rot_mat = glm::rotate(rot_mat, glm::radians(rotation[1]), { 0.0f, 1.0f, 0.0f });
	rot_mat = glm::rotate(rot_mat, glm::radians(rotation[2]), { 0.0f, 0.0f, 1.0f });

	glm::vec3 translation = position;
	if (flip_y) translation[1] *= -1.0f;
	trans_mat = glm::translate(trans_mat, translation);

	if (type == CameraType_FirstPerson)
	{
		matrices.view = rot_mat * trans_mat;
	}
	else 
	{
		matrices.view = trans_mat * rot_mat;
	}

	view_pos = glm::vec4{ position[0], position[1], position[2], 0.0f } * glm::vec4{ -1.0f, 1.0f, -1.0f, 1.0f };

	updated = true;
}