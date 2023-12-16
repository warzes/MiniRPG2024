#pragma once

#include "RenderResources.h"

struct RenderData;

class Render
{
public:
	bool Create(void* glfwWindow, unsigned frameBufferWidth, unsigned frameBufferHeight);
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
	unsigned m_frameWidth = 0;
	unsigned m_frameHeight = 0;
};