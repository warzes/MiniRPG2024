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
#include "Texture.h"
#include "gltf_model.h"

//-----------------------------------------------------------------------------
// Using Bind Groups
// Bind groups are used to pass data to shader binding points. This example sets up bind groups& layouts, creates a single render pipeline based on the bind group layout and renders multiple objects with different bind groups.
//-----------------------------------------------------------------------------

static bool animate = true;

camera_t Camera;

struct view_matrices_t {
	glm::mat4 projection;
	glm::mat4 view;
	glm::mat4 model;
};

struct cube_t {
	view_matrices_t matrices;
	BindGroup bind_group;
	texture_t texture;
	UniformBuffer uniform_buffer;
	glm::vec3 rotation;
};
static cube_t cubes[2] = {};

static gltf_model_t* model = NULL;

static RenderPipeline pipeline;
static PipelineLayout pipeline_layout;

static BindGroupLayout bind_group_layout;

VertexBufferLayout layout;

// Other variables
static const char* example_title = "Using Bind Groups";
static bool prepared = false;

void update_uniform_buffers(wgpu::Queue queue)
{
	static glm::vec3 translations[2] = {
		{-2.0f, 0.0f, 0.0f}, /* Cube 1 */
		{ 1.5f, 0.5f, 0.0f}, /* Cube 2 */
	};

	cube_t* cube = NULL;
	for (uint8_t i = 0; i < 2/*(uint8_t)ARRAY_SIZE(cubes)*/; ++i)
	{
		cube = &cubes[i];
		cube->matrices.model = glm::translate(glm::mat4(1.0f), translations[i]);
		cube->matrices.model = glm::rotate(cube->matrices.model, glm::radians(cube->rotation[0]), { 1.0f, 0.0f, 0.0f });
		cube->matrices.model = glm::rotate(cube->matrices.model, glm::radians(cube->rotation[1]), { 0.0f, 1.0f, 0.0f });
		cube->matrices.model = glm::rotate(cube->matrices.model, glm::radians(cube->rotation[2]), { 0.0f, 0.0f, 1.0f });
		cube->matrices.model = glm::scale(cube->matrices.model, { 0.25f, 0.25f, 0.25f });

		cube->matrices.projection = Camera.matrices.perspective;
		cube->matrices.view = Camera.matrices.view;

		queue.WriteBuffer(cube->uniform_buffer.buffer, 0, &cube->matrices, sizeof(view_matrices_t));
	}
}

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
		Camera.SetPerspective(60.0f, 1024.0f / 768.0f, 0.1f, 512.0f);
		Camera.SetRotation({ 0.0f, 0.0f, 0.0f });
		Camera.SetPosition({ 0.0f, 0.0f, 5.0f });
	}

	// load_assets
	{
		const uint32_t gltf_loading_flags
			= WGPU_GLTF_FileLoadingFlags_PreTransformVertices
			| WGPU_GLTF_FileLoadingFlags_PreMultiplyVertexColors
			| WGPU_GLTF_FileLoadingFlags_DontLoadImages;

		wgpu_gltf_model_load_options_t opt{};
		opt.filename = "../Data/Models/cube.gltf";
		opt.file_loading_flags = gltf_loading_flags;
		model = wgpu_gltf_model_load_from_file(&opt);
		cubes[0].texture = wgpu_create_texture_from_file(m_data->device, "../Data/Textures/crate01_color_height_rgba.ktx", NULL);
		cubes[1].texture = wgpu_create_texture_from_file(m_data->device, "../Data/Textures/crate02_color_height_rgba.ktx", NULL);
	}

	// prepare_uniform_buffers
	{
		// Vertex shader matrix uniform buffer block
		for (uint8_t i = 0; i < 2/*(uint8_t)ARRAY_SIZE(cubes)*/; ++i) 
		{
			cube_t* cube = &cubes[i];
			cube->uniform_buffer.Create(m_data->device, sizeof(view_matrices_t), nullptr);
		}
	}

	// update_uniform_buffers
	update_uniform_buffers(m_data->queue);

	// setup_bind_groups
	{
		/*
		* Bind group layout
		*
		* The layout describes the shader bindings and types used for a certain
		* descriptor layout and as such must match the shader bindings
		*
		* Shader bindings used in this example:
		*
		* VS:
		*    layout (set = 0, binding = 0) uniform UBOMatrices
		*
		* FS:
		*    layout (set = 0, binding = 1) uniform texture2D ...;
		*    layout (set = 0, binding = 2) uniform sampler ...;
		*/
		wgpu::BindGroupLayoutEntry bind_group_layout_entries[3] = {};

		// Binding 0: Uniform buffers (used to pass matrices)
		bind_group_layout_entries[0].binding = 0;
		bind_group_layout_entries[0].visibility = wgpu::ShaderStage::Vertex;
		bind_group_layout_entries[0].buffer.type = wgpu::BufferBindingType::Uniform;
		bind_group_layout_entries[0].buffer.hasDynamicOffset = false;
		bind_group_layout_entries[0].buffer.minBindingSize = sizeof(view_matrices_t);

		// Binding 1: Image view (used to pass per object texture information)
		bind_group_layout_entries[1].binding = 1;
		bind_group_layout_entries[1].visibility = wgpu::ShaderStage::Fragment;
		bind_group_layout_entries[1].texture.sampleType = wgpu::TextureSampleType::Float;
		bind_group_layout_entries[1].texture.viewDimension = wgpu::TextureViewDimension::e2D;
		bind_group_layout_entries[1].texture.multisampled = false;

		// Binding 2: Image sampler (used to pass per object texture information)
		bind_group_layout_entries[2].binding = 2;
		bind_group_layout_entries[2].visibility = wgpu::ShaderStage::Fragment;
		bind_group_layout_entries[2].sampler.type = wgpu::SamplerBindingType::Filtering;

		wgpu::BindGroupLayoutDescriptor bindGroupLayoutDesc{};
		bindGroupLayoutDesc.entryCount = 3;
		bindGroupLayoutDesc.entries = bind_group_layout_entries;
		bind_group_layout.layout = m_data->device.CreateBindGroupLayout(&bindGroupLayoutDesc);

		/*
		 * Bind groups
		 *
		 * Using the shared bind group layout we will now allocate the bind groups.
		 *
		 * Bind groups contain the actual descriptor for the objects (buffers, images)
		 * used at render time.
		 */
		for (uint8_t i = 0; i < 2/*(uint8_t)ARRAY_SIZE(cubes)*/; ++i)
		{
			cube_t* cube = &cubes[i];

			wgpu::BindGroupEntry bindings[3] = {};
			// Binding 0: Object matrices Uniform buffer
			bindings[0].binding = 0;
			bindings[0].buffer = cube->uniform_buffer.buffer;
			bindings[0].offset = 0;
			bindings[0].size = sizeof(view_matrices_t);

			// Binding 1: Object texture view
			bindings[1].binding = 1;
			bindings[1].textureView = cube->texture.view;

			// Binding 2: Object texture sampler
			bindings[2].binding = 2;
			bindings[2].sampler = cube->texture.sampler;

			wgpu::BindGroupDescriptor bindGroupDesc{};
			bindGroupDesc.layout = bind_group_layout.layout;
			bindGroupDesc.entryCount = 3;// (uint32_t)ARRAY_SIZE(bind_group_entries);
			bindGroupDesc.entries = bindings;
			cube->bind_group.bindGroup = m_data->device.CreateBindGroup(&bindGroupDesc);
		}
	}

	// prepare_pipelines
	{
		//wgpu::PipelineLayoutDescriptor d{};
		//d.bindGroupLayoutCount = 1;
		//d.bindGroupLayouts = &bind_group_layout.layout;
		//wgpu::PipelineLayout layout = m_data->device.CreatePipelineLayout(&d);

		layout.SetVertexSize(sizeof(gltfVertex));
		// Location 0: Position
		layout.AddAttrib(wgpu::VertexFormat::Float32x4, offsetof(gltfVertex, position));
		// Location 1: Vertex normal
		layout.AddAttrib(wgpu::VertexFormat::Float32x3, offsetof(gltfVertex, normal));
		// Location 2: Texture coordinates
		layout.AddAttrib(wgpu::VertexFormat::Float32x3, offsetof(gltfVertex, color));
		// Location 3: Vertex color
		layout.AddAttrib(wgpu::VertexFormat::Float32x2, offsetof(gltfVertex, uv));


		const char* shaderText = R"(
struct UBOMatrices {
	projection : mat4x4<f32>,
	view : mat4x4<f32>,
	model : mat4x4<f32>,
};

@group(0) @binding(0) var<uniform> uboMatrices : UBOMatrices;

struct Output {
	@builtin(position) position : vec4<f32>,
	@location(0) normal : vec3<f32>,
	@location(1) color : vec3<f32>,
	@location(2) uv : vec2<f32>,
};

@vertex
fn vs_main(
	@location(0) inPos: vec3<f32>,
	@location(1) inNormal: vec3<f32>,
	@location(2) inUV: vec2<f32>,
	@location(3) inColor: vec3<f32>
) -> Output {
	var output: Output;
	output.normal = inNormal;
	output.color = inColor;
	output.uv = inUV;
	output.position = uboMatrices.projection * uboMatrices.view * uboMatrices.model * vec4<f32>(inPos.xyz, 1.0);
	return output;
}

@group(0) @binding(1) var textureColorMap: texture_2d<f32>;
@group(0) @binding(2) var samplerColorMap: sampler;

@fragment
fn fs_main(
	@location(0) inNormal : vec3<f32>,
	@location(1) inColor : vec3<f32>,
	@location(2) inUV : vec2<f32>
) -> @location(0) vec4<f32> {
	return textureSample(textureColorMap, samplerColorMap, inUV) * vec4<f32>(inColor, 1.0);
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

		pipeline.SetPrimitiveState(wgpu::PrimitiveTopology::TriangleList, wgpu::IndexFormat::Undefined, wgpu::FrontFace::CCW, wgpu::CullMode::None);
		pipeline.SetBlendState(m_data->swapChainFormat);
		pipeline.SetDepthStencilState(depthStencilState);
		pipeline.SetVertexBufferLayout(layout);
		pipeline.SetVertexShaderCode(shaderModule);
		pipeline.SetFragmentShaderCode(shaderModule);
		pipeline.Create(m_data->device);
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
		if (animate)
		{
			cubes[0].rotation[0] += 2.5f * glfwGetTime();
			if (cubes[0].rotation[0] > 360.0f) 
			{
				cubes[0].rotation[0] -= 360.0f;
			}
			cubes[1].rotation[1] += 2.0f * glfwGetTime();
			if (cubes[1].rotation[1] > 360.0f) {
				cubes[1].rotation[1] -= 360.0f;
			}
			update_uniform_buffers(m_data->queue);
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
		renderPass.SetViewport(0.0f, 0.0f, m_frameWidth, m_frameHeight, 0.0f, 1.0f);
		renderPass.SetScissorRect(0, 0, m_frameWidth, m_frameHeight);
		renderPass.SetPipeline(pipeline);
		for (uint64_t i = 0; i < 2; ++i)
		{
			renderPass.SetBindGroup(0, cubes[i].bind_group);
			wgpu_gltf_model_draw(model,{});
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