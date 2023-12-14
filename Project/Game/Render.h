#pragma once

struct RenderData;

class Render
{
public:
	bool Create(void* glfwWindow);
	void Destroy();

	void Frame();

	bool Resize(int width, int height);
	// Mouse events
	void OnMouseMove(double xpos, double ypos);
	void OnMouseButton(int button, int action, int mods, double xpos, double ypos);
	void OnScroll(double xoffset, double yoffset);

private:
	bool createDevice(void* glfwWindow);
	bool initSwapChain(int width, int height);
	void terminateSwapChain();
	bool initDepthBuffer(int width, int height);
	void terminateDepthBuffer();
	void updateProjectionMatrix(int width, int height);
	void updateViewMatrix();

	RenderData* m_data = nullptr;
};