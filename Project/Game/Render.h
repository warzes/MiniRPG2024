#pragma once

struct RenderData;

class Render
{
public:
	bool Create(void* glfwWindow);
	void Destroy();

	void Frame();

private:
	bool createDevice(void* glfwWindow);


	RenderData* m_data = nullptr;
};