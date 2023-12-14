#pragma once

struct RenderData;

class Render
{
public:
	bool Create(void* glfwWindow);
	void Destroy();

	void Frame();

	bool Resize(int width, int height);

private:
	bool createDevice(void* glfwWindow);
	bool initSwapChain(int width, int height);
	void terminateSwapChain();
	bool initDepthBuffer(int width, int height);
	void terminateDepthBuffer();

	RenderData* m_data = nullptr;
};