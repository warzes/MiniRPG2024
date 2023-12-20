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
#include "ExampleMesh.h"

//-----------------------------------------------------------------------------
// Two Cubes
//  This example shows some of the alignment requirements involved when updating and binding multiple slices of a uniform buffer.It renders two rotating cubes which have transform matrices at different offsets in a uniform buffer.
//-----------------------------------------------------------------------------

#define NUMBER_OF_CUBES 3ull
// Settings
static struct 
{
	uint64_t number_of_cubes;
	bool render_bundles;
} settings = {
  .number_of_cubes = NUMBER_OF_CUBES,
  .render_bundles = false,
};


// Cube mesh
static cube_mesh_t cube_mesh = { 0 };

// Cube struct
struct cube_t {
	BindGroup uniform_buffer_bind_group;
	struct {
		glm::mat4 model;
		glm::mat4 model_view_projection;
		glm::mat4 tmp;
	} view_mtx;
};
static cube_t cubes[NUMBER_OF_CUBES] = {};


// Vertex buffer
static VertexBuffer vertices = {};

// Uniform buffer object
static struct {
	UniformBuffer buffer;
	uint64_t offset;
	uint64_t size;
	uint64_t size_with_offset;
} uniform_buffer = {};

static struct {
	glm::mat4 projection;
	glm::mat4 view;
} view_matrices = {};

// Pipeline
static RenderPipeline pipeline;

// Render bundle
static RenderBundle render_bundle;

// Other variables
static const char* example_title = "Two Cubes";
static bool prepared = false;

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

	cube_mesh_init(&cube_mesh);
	vertices.Create(m_data->device, sizeof(cube_mesh.vertex_array), cube_mesh.vertex_array);

	const char* shaderText = R"(
struct Uniforms {
	modelViewProjectionMatrix : mat4x4<f32>,
}
@binding(0) @group(0) var<uniform> uniforms : Uniforms;

struct VertexOutput {
	@builtin(position) Position : vec4<f32>,
	@location(0) fragUV : vec2<f32>,
	@location(1) fragPosition: vec4<f32>,
}

@vertex
fn vs_main(
	@location(0) position : vec4<f32>,
	@location(1) uv : vec2<f32>
) -> VertexOutput {
	var output : VertexOutput;
	output.Position = uniforms.modelViewProjectionMatrix * position;
	output.fragUV = uv;
	output.fragPosition = 0.5 * (position + vec4(1.0, 1.0, 1.0, 1.0));
	return output;
}

@fragment
fn fs_main(
	@location(0) fragUV: vec2<f32>,
	@location(1) fragPosition: vec4<f32>
) -> @location(0) vec4<f32> {
	return fragPosition;
}
)";
	wgpu::ShaderModule shaderModule = CreateShaderModule(m_data->device, shaderText);

	wgpu::DepthStencilState depthStencilState{};
	depthStencilState.depthCompare = wgpu::CompareFunction::LessEqual;
	depthStencilState.depthWriteEnabled = true;
	depthStencilState.format = m_data->depthTextureFormat;
	// Deactivate the stencil alltogether
	depthStencilState.stencilReadMask = 0;
	depthStencilState.stencilWriteMask = 0;

	// Vertex buffer layout
	VertexBufferLayout layout;
	layout.SetVertexSize(cube_mesh.vertex_size);
	layout.AddAttrib(wgpu::VertexFormat::Float32x4, cube_mesh.position_offset);
	layout.AddAttrib(wgpu::VertexFormat::Float32x4, cube_mesh.color_offset);

	pipeline.SetPrimitiveState(wgpu::PrimitiveTopology::TriangleList, wgpu::IndexFormat::Undefined, wgpu::FrontFace::CCW, wgpu::CullMode::None);
	pipeline.SetBlendState(m_data->swapChainFormat);
	pipeline.SetDepthStencilState(depthStencilState);
	pipeline.SetVertexBufferLayout(layout);
	pipeline.SetVertexShaderCode(shaderModule);
	pipeline.SetFragmentShaderCode(shaderModule);
	pipeline.Create(m_data->device);


	// prepare_view_matrices
	{
		const float aspect_ratio = (float)1024 / (float)768;

		// Projection matrix
		view_matrices.projection = glm::perspective(PI2 / 5.0f, aspect_ratio, 1.0f, 100.0f);


		// View matrix
		view_matrices.view = glm::translate(glm::mat4(1.0f), { 0.0f, 0.0f, 7.0f });

		const float start_x = -2.0f, increment_x = 4.0f;
		cube_t* cube = NULL;
		float x = 0.0f;
		for (uint64_t i = 0; i < settings.number_of_cubes; ++i)
		{
			cube = &cubes[i];
			x = start_x + i * increment_x;

			// Model matrices
			cube->view_mtx.model = glm::translate(glm::mat4(1.0f), { x, 0.0f, 0.0f });

			// Model view matrices
			cube->view_mtx.model_view_projection = glm::mat4(1.0f);

			// Temporary matrices
			cube->view_mtx.tmp = glm::mat4(1.0f);
		}
	}

	// prepare_uniform_buffer
	{
		// Unform buffer
		uniform_buffer.size = sizeof(glm::mat4); // 4x4 matrix
		uniform_buffer.offset = 256; // uniformBindGroup offset must be 256-byte aligned
		uniform_buffer.size_with_offset
			= ((settings.number_of_cubes - 1) * uniform_buffer.offset)
			+ uniform_buffer.size;

		uniform_buffer.buffer.Create(m_data->device, uniform_buffer.size_with_offset, nullptr);
	}

	// setup_bind_groups
	{
		for (uint64_t i = 0; i < settings.number_of_cubes; ++i) 
		{
			wgpu::BindGroupEntry bindings{};
			// Binding 0 : Uniform buffer
			bindings.binding = 0;
			bindings.buffer = uniform_buffer.buffer.buffer;
			bindings.offset = i * uniform_buffer.offset;
			bindings.size = uniform_buffer.size;

			wgpu::BindGroupDescriptor bindGroupDesc{};
			bindGroupDesc.layout = pipeline.pipeline.GetBindGroupLayout(0);
			bindGroupDesc.entryCount = 1;
			bindGroupDesc.entries = &bindings;
			cubes[i].uniform_buffer_bind_group.bindGroup = m_data->device.CreateBindGroup(&bindGroupDesc);
		}
	}	

	// prepare_render_bundle_encoder
	{
		wgpu::TextureFormat color_formats[1] = { m_data->swapChainFormat };
		wgpu::RenderBundleEncoderDescriptor encoderDesc{};
		encoderDesc.colorFormatCount = 1;//(uint32_t)ARRAY_SIZE(color_formats);
		encoderDesc.colorFormats = color_formats;
		encoderDesc.depthStencilFormat = m_data->depthTextureFormat;
		encoderDesc.sampleCount = 1;

		wgpu::RenderBundleEncoder render_bundle_encoder = m_data->device.CreateRenderBundleEncoder(&encoderDesc);
		if (render_bundle_encoder)
		{
			render_bundle_encoder.SetPipeline(pipeline.pipeline);
			render_bundle_encoder.SetVertexBuffer(0, vertices.buffer);
			for (uint64_t i = 0; i < settings.number_of_cubes; ++i) {
				render_bundle_encoder.SetBindGroup(0, cubes[i].uniform_buffer_bind_group.bindGroup);
				render_bundle_encoder.Draw(cube_mesh.vertex_count, 1, 0, 0);
			}
		}
		render_bundle.bundle = render_bundle_encoder.Finish();
		render_bundle_encoder = nullptr;
	}

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
	// update_transformation_matrix
	{

		const float now = glfwGetTime();
		const float sin_now = sin(now), cos_now = cos(now);

		cube_t* cube = NULL;
		for (uint64_t i = 0; i < settings.number_of_cubes; ++i)
		{
			cube = &cubes[i];
			cube->view_mtx.tmp = cube->view_mtx.model;
			if (i % 2 == 0)
			{
				cube->view_mtx.tmp = glm::rotate(cube->view_mtx.tmp, 1.0f, { sin_now, cos_now, 0.0f });
			}
			else if (i % 2 == 1)
			{
				cube->view_mtx.tmp = glm::rotate(cube->view_mtx.tmp, 1.0f, { cos_now, sin_now, 0.0f });
			}
			cube->view_mtx.model_view_projection = view_matrices.projection * view_matrices.view * cube->view_mtx.tmp;
		}

		for (uint64_t i = 0; i < settings.number_of_cubes; ++i)
		{
			m_data->queue.WriteBuffer(uniform_buffer.buffer.buffer, i * uniform_buffer.offset, &cubes[i].view_mtx.model_view_projection, sizeof(glm::mat4));
		}
	}

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
		if (settings.render_bundles)
		{
			renderPass.ExecuteBundles(1, &render_bundle);
		}
		else 
		{
			renderPass.SetViewport(0.0f, 0.0f, m_frameWidth, m_frameHeight, 0.0f, 1.0f);
			renderPass.SetScissorRect(0, 0, m_frameWidth, m_frameHeight);
			renderPass.SetPipeline(pipeline);
			renderPass.SetVertexBuffer(0, vertices);
			for (uint64_t i = 0; i < settings.number_of_cubes; ++i)
			{
				renderPass.SetBindGroup(0, cubes[i].uniform_buffer_bind_group);
				renderPass.Draw(cube_mesh.vertex_count, 1, 0, 0);
			}
		}
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