#include "Engine.h"
#include <Dawn/webgpu.h>
#include <dawn/webgpu_cpp.h>
#include <Dawn/native/DawnNative.h>
#include <webgpu/webgpu_glfw.h>
#include <Dawn/utils/WGPUHelpers.h>
#include <Dawn/dawn_proc.h>
#include <glfw.h>

#include "RenderUtils.h"
#include "RenderCore.h"
#include "RenderResources.h"
#include "RenderModel.h"
#include "Examples.h"
//-----------------------------------------------------------------------------
// Triangle
// Basic and verbose example for getting a colored triangle rendered to the screen using WebGPU. 
//-----------------------------------------------------------------------------
RenderPipeline pipeline;
VertexBuffer vb;
IndexBuffer ib;
UniformBuffer ub;
wgpu::BindGroupEntry bindings{};
BindGroup bindGroup;
wgpu::BindGroupLayoutEntry bindingLayout{};
wgpu::BindGroupLayoutDescriptor bindGroupLayoutDesc{};
wgpu::BindGroupDescriptor bindGroupDesc{};
BindGroupLayout bindGroupLayout;
wgpu::PipelineLayoutDescriptor layoutDesc{};
PipelineLayout pipelineLayout;
VertexBufferLayout layout;

struct Vertex
{
	glm::vec3 position;
	glm::vec3 color;
};
struct Uniform
{
	glm::mat4 projection = glm::mat4(1.0f);
	glm::mat4 model = glm::mat4(1.0f);
	glm::mat4 view = glm::mat4(1.0f);
} ubo_vs;
static_assert(sizeof(Uniform) % 16 == 0);

camera_t Camera;
//-----------------------------------------------------------------------------
struct RenderData
{
	// Device
	std::unique_ptr<dawn::native::Instance> dawnInstance = nullptr;
	wgpu::Device device = nullptr;
	wgpu::Queue queue = nullptr;

	// Swap Chain
	wgpu::Surface surface = nullptr;
	wgpu::TextureFormat swapChainFormat = wgpu::TextureFormat::Undefined;
	wgpu::SwapChain swapChain = nullptr;

	// Depth Buffer
	wgpu::Texture depthTexture = nullptr;
	wgpu::TextureView depthTextureView = nullptr;
	wgpu::TextureFormat depthTextureFormat = wgpu::TextureFormat::Depth24Plus;
};
//-----------------------------------------------------------------------------
bool Render::Create(void* glfwWindow, unsigned frameBufferWidth, unsigned frameBufferHeight)
{
	m_data = new RenderData();
	m_frameWidth = frameBufferWidth;
	m_frameHeight = frameBufferHeight;
	if (!createDevice(glfwWindow))
		return false;
	if (!initSwapChain(m_frameWidth, m_frameHeight))
		return false;
	if (!initDepthBuffer(m_frameWidth, m_frameHeight))
		return false;

	{
		Camera.type = CameraType_LookAt;
		Camera.SetPosition({ 0.0f, 0.0f, 2.5f });
		Camera.SetRotation({ 0.0f, 0.0f, 0.0f });
		Camera.SetPerspective(60.0f, 1024.0f / 768.0f, 0.0f, 256.0f);
	}

	{
		// Setup vertices (x, y, z, r, g, b)
		static const Vertex vertex_buffer[] = 
		{
		  {
			.position = {1.0f, -1.0f, 0.0f},
			.color = {1.0f, 0.0f, 0.0f},
		  },
		  {
			.position = {-1.0f, -1.0f, 0.0f},
			.color = {0.0f, 1.0f, 0.0f},
		  },
		  {
			.position = {0.0f, 1.0f, 0.0f},
			.color = {0.0f, 0.0f, 1.0f},
		  },
		};
		vb.Create(m_data->device, 3, sizeof(Vertex), vertex_buffer);

		// Setup indices
		static const uint32_t index_buffer[] = {
		  0, 1, 2
		};
		ib.Create(m_data->device, 3, sizeof(uint32_t), index_buffer);
	}

	{
		ub.Create(m_data->device, sizeof(ubo_vs), &ubo_vs);
	}

	// Bind group layout
	{
		bindingLayout.binding = 0;
		bindingLayout.visibility = wgpu::ShaderStage::Vertex;
		bindingLayout.buffer.type = wgpu::BufferBindingType::Uniform;
		bindingLayout.buffer.hasDynamicOffset = false;
		bindingLayout.buffer.minBindingSize = sizeof(Uniform);

		bindGroupLayoutDesc.entryCount = 1;
		bindGroupLayoutDesc.entries = &bindingLayout;
		bindGroupLayout.layout = m_data->device.CreateBindGroupLayout(&bindGroupLayoutDesc);
	}

	// Create the pipeline layout
	{		
		layoutDesc.bindGroupLayoutCount = 1;
		layoutDesc.bindGroupLayouts = &bindGroupLayout.layout;
		pipelineLayout.layout = m_data->device.CreatePipelineLayout(&layoutDesc);
	}

	// Bind Group
	{
		// Binding 0 : Uniform buffer
		bindings.binding = 0;
		bindings.buffer = ub.buffer;
		bindings.offset = 0;
		bindings.size = sizeof(Uniform);

		bindGroupDesc.layout = bindGroupLayout.layout;
		bindGroupDesc.entryCount = 1;
		bindGroupDesc.entries = &bindings;
		bindGroup.bindGroup = m_data->device.CreateBindGroup(&bindGroupDesc);
	}

	layout.SetVertexSize(sizeof(Vertex));
	layout.AddAttrib(wgpu::VertexFormat::Float32x3, offsetof(Vertex, position));
	layout.AddAttrib(wgpu::VertexFormat::Float32x3, offsetof(Vertex, color));

	const char* shaderText = R"(
struct UBO {
	projectionMatrix : mat4x4<f32>,
	modelMatrix      : mat4x4<f32>,
	viewMatrix       : mat4x4<f32>,
}

@group(0) @binding(0) var<uniform> ubo : UBO;

struct VertexInput {
	@location(0) position : vec3<f32>,
	@location(1) color : vec3<f32>
};

struct VertexOutput {
	@builtin(position) position : vec4<f32>,
	@location(0) fragColor : vec3<f32>
};

@vertex
fn vs_main(vertex  : VertexInput) -> VertexOutput
{
	var output : VertexOutput;
	output.position = ubo.projectionMatrix * ubo.viewMatrix * ubo.modelMatrix * vec4<f32>(vertex.position.xyz, 1.0);
	output.fragColor = vertex.color;
	return output;
};

struct FragmentInput {
	@location(0) fragColor : vec3<f32>
};

struct FragmentOutput {
	@location(0) outColor : vec4<f32>
};

@fragment
fn fs_main(fragment : FragmentInput) -> FragmentOutput
{
	var output : FragmentOutput;
	output.outColor = vec4<f32>(fragment.fragColor, 1.0);
	return output;
};
)";
	wgpu::ShaderModule shaderModule = CreateShaderModule(m_data->device, shaderText);

	wgpu::DepthStencilState depthStencilState{};
	depthStencilState.depthCompare = wgpu::CompareFunction::LessEqual;
	depthStencilState.depthWriteEnabled = true;
	depthStencilState.format = m_data->depthTextureFormat;
	// Deactivate the stencil alltogether
	depthStencilState.stencilReadMask = 0;
	depthStencilState.stencilWriteMask = 0;

	pipeline.SetPrimitiveState(wgpu::PrimitiveTopology::TriangleList, wgpu::IndexFormat::Undefined, wgpu::FrontFace::CCW, wgpu::CullMode::None);
	pipeline.SetBlendState(m_data->swapChainFormat);
	pipeline.SetDepthStencilState(depthStencilState);
	pipeline.SetVertexBufferLayout(layout);
	pipeline.SetVertexShaderCode(shaderModule);
	pipeline.SetFragmentShaderCode(shaderModule);
	pipeline.Create(m_data->device);

	return true;
}
//-----------------------------------------------------------------------------
void Render::Destroy()
{
	terminateDepthBuffer();
	terminateSwapChain();
	m_data->dawnInstance.reset();
	delete m_data;
}
//-----------------------------------------------------------------------------
void Render::Frame()
{
	ubo_vs.projection = Camera.matrices.perspective;
	ubo_vs.view = Camera.matrices.view;
	ubo_vs.model = glm::mat4(1.0f);
	// Map uniform buffer and update it
	m_data->queue.WriteBuffer(ub.buffer, 0, &ubo_vs, sizeof(ubo_vs));

	wgpu::TextureView backbufferView = m_data->swapChain.GetCurrentTextureView();
	if (!backbufferView)
	{
		Fatal("Cannot acquire next swap chain texture");
		return;
	}

	RenderPass renderPass;
	renderPass.SetTextureView(backbufferView, m_data->depthTextureView);

	wgpu::CommandEncoder encoder = m_data->device.CreateCommandEncoder();
	{
		renderPass.Start(encoder);
		renderPass.SetViewport(0.0f, 0.0f, m_frameWidth, m_frameHeight, 0.0f, 1.0f);
		renderPass.SetScissorRect(0, 0, m_frameWidth, m_frameHeight);
		renderPass.SetPipeline(pipeline);
		renderPass.SetBindGroup(0, bindGroup, 0, nullptr);
		renderPass.SetVertexBuffer(0, vb);
		//renderPass.SetIndexBuffer(ib, wgpu::IndexFormat::Uint32);
		renderPass.Draw(3, 1);
		renderPass.End();
	}

	wgpu::CommandBuffer commands = encoder.Finish();
	m_data->queue.Submit(1, &commands);

	m_data->swapChain.Present();
	m_data->device.Tick(); // ??? только для Dawn?
}
//-----------------------------------------------------------------------------
bool Render::createDevice(void* glfwWindow)
{
#ifdef WEBGPU_BACKEND_WGPU
	m_data->swapChainFormat = m_surface.getPreferredFormat(adapter);
#else
	m_data->swapChainFormat = wgpu::TextureFormat::BGRA8Unorm;
#endif

	WGPUInstanceDescriptor instanceDescriptor{};
	instanceDescriptor.features.timedWaitAnyEnable = true;
	m_data->dawnInstance = std::make_unique<dawn::native::Instance>(&instanceDescriptor);
	if (!m_data->dawnInstance)
	{
		puts("Could not initialize WebGPU!");
		return false;
	}

	std::vector<dawn::native::Adapter> adapters = m_data->dawnInstance->EnumerateAdapters();
	dawn::native::Adapter* preferredAdapter = nullptr;
	wgpu::AdapterProperties properties;

	// find DiscreteGPU
	for (size_t i = 0; i < adapters.size(); i++)
	{
		adapters[i].GetProperties(&properties);
		if (properties.adapterType == wgpu::AdapterType::DiscreteGPU)
		{
			preferredAdapter = &adapters[i];
			break;
		}
	}
	// else find IntegratedGPU
	if (!preferredAdapter)
	{
		for (size_t i = 0; i < adapters.size(); i++)
		{
			adapters[i].GetProperties(&properties);
			if (properties.adapterType == wgpu::AdapterType::IntegratedGPU)
			{
				preferredAdapter = &adapters[i];
				break;
			}
		}
	}
	// else
	if (!preferredAdapter)
	{
		Error("Failed to find an adapter!");
		preferredAdapter = &adapters[0];
		preferredAdapter->GetProperties(&properties);
	}

	WGPUDevice backendDevice = preferredAdapter->CreateDevice();
	DawnProcTable backendProcs = dawn::native::GetProcs();

	dawnProcSetProcs(&backendProcs);
	backendProcs.deviceSetUncapturedErrorCallback(backendDevice, wgpuPrintDeviceError, nullptr);
	backendProcs.deviceSetDeviceLostCallback(backendDevice, wgpuDeviceLostCallback, nullptr);
	backendProcs.deviceSetLoggingCallback(backendDevice, wgpuDeviceLogCallback, nullptr);

	auto surfaceChainedDesc = wgpu::glfw::SetupWindowAndGetSurfaceDescriptor((GLFWwindow*)glfwWindow);
	WGPUSurfaceDescriptor surfaceDesc{};
	surfaceDesc.nextInChain = reinterpret_cast<WGPUChainedStruct*>(surfaceChainedDesc.get());
	WGPUSurface surface = wgpuInstanceCreateSurface(m_data->dawnInstance->Get(), &surfaceDesc);

	m_data->device = wgpu::Device::Acquire(backendDevice);
	m_data->surface = wgpu::Surface::Acquire(surface);
	m_data->queue = m_data->device.GetQueue();

	// print adapter property
	{
		wgpu::AdapterProperties adapterProperties{};
		m_data->device.GetAdapter().GetProperties(&adapterProperties);
		Print("Adapter properties:");
		Print(" - vendorID: " + std::to_string(adapterProperties.vendorID));
		Print(" - deviceID: " + std::to_string(adapterProperties.deviceID));
		Print(" - name: " + std::string(adapterProperties.name));
		if (adapterProperties.driverDescription)
			Print(" - driverDescription: " + std::string(adapterProperties.driverDescription));
		//Print(" - adapterType: " + std::string(adapterProperties.adapterType)); // TODO: доделать конвертацию ида в строку
		//Print(" - backendType: " + std::string(adapterProperties.backendType)); // TODO: доделать конвертацию ида в строку
	}
	// print limits
	{
		WGPUSupportedLimits limits{};
		bool success = wgpuDeviceGetLimits(backendDevice, &limits);

		if (success)
		{
			Print("Device limits:");
			Print(" - maxTextureDimension1D: " + std::to_string(limits.limits.maxTextureDimension1D));
			Print(" - maxTextureDimension2D: " + std::to_string(limits.limits.maxTextureDimension2D));
			Print(" - maxTextureDimension3D: " + std::to_string(limits.limits.maxTextureDimension3D));
			Print(" - maxTextureArrayLayers: " + std::to_string(limits.limits.maxTextureArrayLayers));
			Print(" - maxBindGroups: " + std::to_string(limits.limits.maxBindGroups));
			Print(" - maxBindGroupsPlusVertexBuffers: " + std::to_string(limits.limits.maxBindGroupsPlusVertexBuffers));
			Print(" - maxBindingsPerBindGroup: " + std::to_string(limits.limits.maxBindingsPerBindGroup));
			Print(" - maxDynamicUniformBuffersPerPipelineLayout: " + std::to_string(limits.limits.maxDynamicUniformBuffersPerPipelineLayout));
			Print(" - maxDynamicStorageBuffersPerPipelineLayout: " + std::to_string(limits.limits.maxDynamicStorageBuffersPerPipelineLayout));
			Print(" - maxSampledTexturesPerShaderStage: " + std::to_string(limits.limits.maxSampledTexturesPerShaderStage));
			Print(" - maxSamplersPerShaderStage: " + std::to_string(limits.limits.maxSamplersPerShaderStage));
			Print(" - maxStorageBuffersPerShaderStage: " + std::to_string(limits.limits.maxStorageBuffersPerShaderStage));
			Print(" - maxStorageTexturesPerShaderStage: " + std::to_string(limits.limits.maxStorageTexturesPerShaderStage));
			Print(" - maxUniformBuffersPerShaderStage: " + std::to_string(limits.limits.maxUniformBuffersPerShaderStage));
			Print(" - maxUniformBufferBindingSize: " + std::to_string(limits.limits.maxUniformBufferBindingSize));
			Print(" - maxStorageBufferBindingSize: " + std::to_string(limits.limits.maxStorageBufferBindingSize));
			Print(" - minUniformBufferOffsetAlignment: " + std::to_string(limits.limits.minUniformBufferOffsetAlignment));
			Print(" - minStorageBufferOffsetAlignment: " + std::to_string(limits.limits.minStorageBufferOffsetAlignment));
			Print(" - maxVertexBuffers: " + std::to_string(limits.limits.maxVertexBuffers));
			Print(" - maxBufferSize: " + std::to_string(limits.limits.maxBufferSize));
			Print(" - maxVertexAttributes: " + std::to_string(limits.limits.maxVertexAttributes));
			Print(" - maxVertexBufferArrayStride: " + std::to_string(limits.limits.maxVertexBufferArrayStride));
			Print(" - maxInterStageShaderComponents: " + std::to_string(limits.limits.maxInterStageShaderComponents));
			Print(" - maxInterStageShaderVariables: " + std::to_string(limits.limits.maxInterStageShaderVariables));
			Print(" - maxColorAttachments: " + std::to_string(limits.limits.maxColorAttachments));
			Print(" - maxColorAttachmentBytesPerSample: " + std::to_string(limits.limits.maxColorAttachmentBytesPerSample));
			Print(" - maxComputeWorkgroupStorageSize: " + std::to_string(limits.limits.maxComputeWorkgroupStorageSize));
			Print(" - maxComputeInvocationsPerWorkgroup: " + std::to_string(limits.limits.maxComputeInvocationsPerWorkgroup));
			Print(" - maxComputeWorkgroupSizeX: " + std::to_string(limits.limits.maxComputeWorkgroupSizeX));
			Print(" - maxComputeWorkgroupSizeY: " + std::to_string(limits.limits.maxComputeWorkgroupSizeY));
			Print(" - maxComputeWorkgroupSizeZ: " + std::to_string(limits.limits.maxComputeWorkgroupSizeZ));
			Print(" - maxComputeWorkgroupsPerDimension: " + std::to_string(limits.limits.maxComputeWorkgroupsPerDimension));
		}
	}

	return true;
}
//-----------------------------------------------------------------------------
bool Render::Resize(int width, int height)
{
	m_frameWidth = width;
	m_frameHeight = height;

	terminateDepthBuffer();
	terminateSwapChain();

	if (!initSwapChain(width, height)) return false;
	if (!initDepthBuffer(width, height)) return false;

	return true;
}
//-----------------------------------------------------------------------------
bool Render::initSwapChain(int width, int height)
{
	wgpu::SwapChainDescriptor swapChainDesc{};
	swapChainDesc.width = static_cast<uint32_t>(width);
	swapChainDesc.height = static_cast<uint32_t>(height);
	swapChainDesc.usage = wgpu::TextureUsage::RenderAttachment;
	swapChainDesc.format = static_cast<wgpu::TextureFormat>(m_data->swapChainFormat);
	swapChainDesc.presentMode = wgpu::PresentMode::Fifo; // WGPUPresentMode_Mailbox
	m_data->swapChain = m_data->device.CreateSwapChain(m_data->surface, &swapChainDesc);

	return true;
}
//-----------------------------------------------------------------------------
void Render::terminateSwapChain()
{
	m_data->swapChain = nullptr;
}
//-----------------------------------------------------------------------------
bool Render::initDepthBuffer(int width, int height)
{
	// Create the depth texture
	wgpu::TextureDescriptor depthTextureDescriptor{};
	depthTextureDescriptor.dimension = wgpu::TextureDimension::e2D;
	depthTextureDescriptor.format = m_data->depthTextureFormat;
	depthTextureDescriptor.mipLevelCount = 1;
	depthTextureDescriptor.sampleCount = 1;
	depthTextureDescriptor.size = { static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1 };
	depthTextureDescriptor.usage = wgpu::TextureUsage::RenderAttachment;
	depthTextureDescriptor.viewFormatCount = 1;
	depthTextureDescriptor.viewFormats = (wgpu::TextureFormat*)&m_data->depthTextureFormat;
	m_data->depthTexture = m_data->device.CreateTexture(&depthTextureDescriptor);
	if (!m_data->depthTexture)
		return false;

	// Create the view of the depth texture manipulated by the rasterizer
	wgpu::TextureViewDescriptor depthTextureViewDescriptor{};
	depthTextureViewDescriptor.aspect = wgpu::TextureAspect::All;
	depthTextureViewDescriptor.baseArrayLayer = 0;
	depthTextureViewDescriptor.arrayLayerCount = 1;
	depthTextureViewDescriptor.baseMipLevel = 0;
	depthTextureViewDescriptor.mipLevelCount = 1;
	depthTextureViewDescriptor.dimension = wgpu::TextureViewDimension::e2D;
	depthTextureViewDescriptor.format = m_data->depthTextureFormat;
	m_data->depthTextureView = m_data->depthTexture.CreateView(&depthTextureViewDescriptor);

	return m_data->depthTextureView != nullptr;
}
//-----------------------------------------------------------------------------
void Render::terminateDepthBuffer()
{
	if (m_data->depthTexture) m_data->depthTexture.Destroy();
	m_data->depthTexture = nullptr;
	m_data->depthTextureView = nullptr;
}
//-----------------------------------------------------------------------------