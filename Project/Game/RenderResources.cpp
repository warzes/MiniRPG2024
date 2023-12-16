#include "Engine.h"

#include <Dawn/webgpu.h>
#include <dawn/webgpu_cpp.h>
#include <Dawn/native/DawnNative.h>
#include <webgpu/webgpu_glfw.h>
#include <Dawn/utils/WGPUHelpers.h>
#include <Dawn/utils/ComboRenderPipelineDescriptor.h>
#include <Dawn/dawn_proc.h>

struct Buffer::BufferData
{
	~BufferData()
	{
		buffer.Destroy();
		buffer = nullptr;
	}
	wgpu::Buffer buffer = nullptr;
};

Buffer::Buffer()
{
	m_data = new BufferData;
}

Buffer::~Buffer()
{
	delete m_data;
}
