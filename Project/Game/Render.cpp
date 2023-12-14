#include "Engine.h"

#include <Dawn/webgpu.h>
#include <dawn/webgpu_cpp.h>
#include <Dawn/native/DawnNative.h>
#include <webgpu/webgpu_glfw.h>
#include <Dawn/utils/WGPUHelpers.h>
#include <Dawn/utils/ComboRenderPipelineDescriptor.h>
#include <Dawn/dawn_proc.h>
#include <glfw.h>

#include "RenderUtils.h"
#include "RenderCore.h"
#include "RenderResources.h"
#include "RenderModel.h"
//-----------------------------------------------------------------------------
constexpr uint32_t kWidth = 1024;
constexpr uint32_t kHeight = 768;

wgpu::Texture texture;
wgpu::Sampler sampler;

wgpu::RenderPipeline pipeline2;
wgpu::Buffer vertexBuffer2;
wgpu::Buffer indexBuffer2;
wgpu::Buffer uniformBuffer;
wgpu::BindGroup bindGroup2;

struct MyUniforms {
	glm::mat4x4 projectionMatrix;
	glm::mat4x4 viewMatrix;
	glm::mat4x4 modelMatrix;
	std::array<float, 4> color;
	float time;
	float _pad[3];
};
static_assert(sizeof(MyUniforms) % 16 == 0);

MyUniforms uniforms;

int indexCount;
std::vector<VertexAttributes> vertexData;
float angle1;
glm::mat4x4 R1;
glm::mat4x4 T1;
glm::mat4x4 S;

wgpu::Texture texture2;
wgpu::TextureView textureView;
wgpu::Sampler sampler2;
wgpu::BindGroupLayout bindGroupLayout;

struct CameraState {
	// angles.x is the rotation of the camera around the global vertical axis, affected by mouse.x
	// angles.y is the rotation of the camera around its local horizontal axis, affected by mouse.y
	glm::vec2 angles = { 0.8f, 0.5f };
	// zoom is the position of the camera along its local forward axis, affected by the scroll wheel
	float zoom = -1.2f;
};

struct DragState {
	// Whether a drag action is ongoing (i.e., we are between mouse press and mouse release)
	bool active = false;
	// The position of the mouse at the beginning of the drag action
	glm::vec2 startMouse;
	// The camera state at the beginning of the drag action
	CameraState startCameraState;

	// Constant settings
	float sensitivity = 0.01f;
	float scrollSensitivity = 0.1f;

	// Inertia
	glm::vec2 velocity = { 0.0, 0.0 };
	glm::vec2 previousDelta;
	float intertia = 0.9f;
};

CameraState m_cameraState;
DragState m_drag;

bool updateDragInertia()
{
	constexpr float eps = 1e-4f;
	// Apply inertia only when the user released the click.
	if (!m_drag.active)
	{
		// Avoid updating the matrix when the velocity is no longer noticeable
		if (std::abs(m_drag.velocity.x) < eps && std::abs(m_drag.velocity.y) < eps)
			return false;

		m_cameraState.angles += m_drag.velocity;
		m_cameraState.angles.y = glm::clamp(m_cameraState.angles.y, -3.14f / 2 + 1e-5f, 3.14f / 2 - 1e-5f);
		// Dampen the velocity so that it decreases exponentially and stops
		// after a few frames.
		m_drag.velocity *= m_drag.intertia;
		return true;
	}
	return false;
}


bool initRenderPipeline(wgpu::Device device, wgpu::TextureFormat swapChainFormat, wgpu::TextureFormat depthTextureFormat)
{
	const char* shaderText = R"(
struct MyUniforms {
	projectionMatrix: mat4x4f,
	viewMatrix: mat4x4f,
	modelMatrix: mat4x4f,
	color: vec4f,
	time: f32,
};

struct VertexInput {
	@location(0) position: vec3f,
	@location(1) normal: vec3f,
	@location(2) color: vec3f,
	@location(3) uv: vec2f,
};

struct VertexOutput {
	@builtin(position) position: vec4f,
	@location(0) color: vec3f,
	@location(1) normal: vec3f,
	@location(2) uv: vec2f,
};

@group(0) @binding(0) var<uniform> uMyUniforms: MyUniforms;
@group(0) @binding(1) var gradientTexture: texture_2d<f32>;
@group(0) @binding(2) var textureSampler: sampler;

@vertex
fn vs_main(in: VertexInput) -> VertexOutput
{
	var out: VertexOutput;
	out.position = uMyUniforms.projectionMatrix * uMyUniforms.viewMatrix * uMyUniforms.modelMatrix * vec4f(in.position, 1.0);
	out.color = in.color;
	out.normal = (uMyUniforms.modelMatrix * vec4f(in.normal, 0.0)).xyz;
	out.uv = in.uv;
	return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f
{
	//let normal = normalize(in.normal);

	//let lightColor1 = vec3f(1.0, 0.9, 0.6);
	//let lightColor2 = vec3f(0.6, 0.9, 1.0);
	//let lightDirection1 = vec3f(0.5, -0.9, 0.1);
	//let lightDirection2 = vec3f(0.2, 0.4, 0.3);
	//let shading1 = max(0.0, dot(lightDirection1, normal));
	//let shading2 = max(0.0, dot(lightDirection2, normal));
	//let shading = shading1 * lightColor1 + shading2 * lightColor2;
	//let color = in.color * shading;

	let texelCoords = vec2i(in.uv * vec2f(textureDimensions(gradientTexture))); // получение текстурных координат без семпла... дает олдскул пикселявочсть?
	//let color = textureLoad(gradientTexture, texelCoords, 0).rgb;
	let color = textureSample(gradientTexture, textureSampler, in.uv).rgb;

	// Gamma-correction
	let corrected_color = pow(color, vec3f(2.2));
	return vec4f(corrected_color, uMyUniforms.color.a);
}
)";
	wgpu::ShaderModule shaderModule = CreateShaderModule(device, shaderText);

	// Vertex fetch
	std::vector<wgpu::VertexAttribute> vertexAttribs(4);
	// Position attribute
	vertexAttribs[0].shaderLocation = 0;
	vertexAttribs[0].format = wgpu::VertexFormat::Float32x3;
	vertexAttribs[0].offset = offsetof(VertexAttributes, position);
	// Normal attribute
	vertexAttribs[1].shaderLocation = 1;
	vertexAttribs[1].format = wgpu::VertexFormat::Float32x3;
	vertexAttribs[1].offset = offsetof(VertexAttributes, normal);
	// Color attribute
	vertexAttribs[2].shaderLocation = 2;
	vertexAttribs[2].format = wgpu::VertexFormat::Float32x3;
	vertexAttribs[2].offset = offsetof(VertexAttributes, color);
	// UV attribute
	vertexAttribs[3].shaderLocation = 3;
	vertexAttribs[3].format = wgpu::VertexFormat::Float32x2;
	vertexAttribs[3].offset = offsetof(VertexAttributes, uv);

	wgpu::VertexBufferLayout vertexBufferLayout{};
	vertexBufferLayout.attributeCount = static_cast<uint32_t>(vertexAttribs.size());
	vertexBufferLayout.attributes = vertexAttribs.data();
	vertexBufferLayout.arrayStride = sizeof(VertexAttributes);
	vertexBufferLayout.stepMode = wgpu::VertexStepMode::Vertex;

	wgpu::BlendState blendState{};
	blendState.color.srcFactor = wgpu::BlendFactor::SrcAlpha;
	blendState.color.dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha;
	blendState.color.operation = wgpu::BlendOperation::Add;
	blendState.alpha.srcFactor = wgpu::BlendFactor::Zero;
	blendState.alpha.dstFactor = wgpu::BlendFactor::One;
	blendState.alpha.operation = wgpu::BlendOperation::Add;

	wgpu::ColorTargetState colorTarget{};
	colorTarget.format = swapChainFormat;
	colorTarget.blend = &blendState;
	colorTarget.writeMask = wgpu::ColorWriteMask::All;

	wgpu::FragmentState fragmentState{};
	fragmentState.module = shaderModule;
	fragmentState.entryPoint = "fs_main";
	fragmentState.constantCount = 0;
	fragmentState.constants = nullptr;
	fragmentState.targetCount = 1;
	fragmentState.targets = &colorTarget;

	wgpu::DepthStencilState depthStencilState{};
	depthStencilState.depthCompare = wgpu::CompareFunction::Less;
	depthStencilState.depthWriteEnabled = true;
	depthStencilState.format = depthTextureFormat;
	// Deactivate the stencil alltogether
	depthStencilState.stencilReadMask = 0;
	depthStencilState.stencilWriteMask = 0;

	// Since we now have 2 bindings, we use a vector to store them
	std::vector<wgpu::BindGroupLayoutEntry> bindingLayoutEntries(3);

	wgpu::BindGroupLayoutEntry& bindingLayout = bindingLayoutEntries[0];
	// The binding index as used in the @binding attribute in the shader
	bindingLayout.binding = 0;
	// The stage that needs to access this resource
	bindingLayout.visibility = wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment;
	bindingLayout.buffer.type = wgpu::BufferBindingType::Uniform;
	bindingLayout.buffer.minBindingSize = sizeof(MyUniforms);

	wgpu::BindGroupLayoutEntry& textureBindingLayout = bindingLayoutEntries[1];
	// Setup texture binding
	textureBindingLayout.binding = 1;
	textureBindingLayout.visibility = wgpu::ShaderStage::Fragment;
	textureBindingLayout.texture.sampleType = wgpu::TextureSampleType::Float;
	textureBindingLayout.texture.viewDimension = wgpu::TextureViewDimension::e2D;

	// The texture sampler binding
	wgpu::BindGroupLayoutEntry& samplerBindingLayout = bindingLayoutEntries[2];
	samplerBindingLayout.binding = 2;
	samplerBindingLayout.visibility = wgpu::ShaderStage::Fragment;
	samplerBindingLayout.sampler.type = wgpu::SamplerBindingType::Filtering;

	// Create a bind group layout
	wgpu::BindGroupLayoutDescriptor bindGroupLayoutDesc{};
	bindGroupLayoutDesc.entryCount = (uint32_t)bindingLayoutEntries.size();
	bindGroupLayoutDesc.entries = bindingLayoutEntries.data();
	bindGroupLayout = device.CreateBindGroupLayout(&bindGroupLayoutDesc);

	// Create the pipeline layout
	wgpu::PipelineLayoutDescriptor layoutDesc{};
	layoutDesc.bindGroupLayoutCount = 1;
	layoutDesc.bindGroupLayouts = (wgpu::BindGroupLayout*)&bindGroupLayout;
	wgpu::PipelineLayout layout = device.CreatePipelineLayout(&layoutDesc);

	wgpu::RenderPipelineDescriptor pipelineDesc{};

	pipelineDesc.vertex.bufferCount = 1;
	pipelineDesc.vertex.buffers = &vertexBufferLayout;
	pipelineDesc.vertex.module = shaderModule;
	pipelineDesc.vertex.entryPoint = "vs_main";
	pipelineDesc.vertex.constantCount = 0;
	pipelineDesc.vertex.constants = nullptr;

	pipelineDesc.primitive.topology = wgpu::PrimitiveTopology::TriangleList;
	pipelineDesc.primitive.stripIndexFormat = wgpu::IndexFormat::Undefined;
	pipelineDesc.primitive.frontFace = wgpu::FrontFace::CCW;
	pipelineDesc.primitive.cullMode = wgpu::CullMode::None;

	pipelineDesc.fragment = &fragmentState;

	pipelineDesc.depthStencil = &depthStencilState;

	pipelineDesc.multisample.count = 1;
	pipelineDesc.multisample.mask = ~0u;
	pipelineDesc.multisample.alphaToCoverageEnabled = false;

	pipelineDesc.layout = layout;

	pipeline2 = device.CreateRenderPipeline(&pipelineDesc);

	return pipeline2 != nullptr;
}

bool initTexture(wgpu::Device device)
{
	// Create a sampler
	wgpu::SamplerDescriptor samplerDesc;
	samplerDesc.addressModeU = wgpu::AddressMode::Repeat;
	samplerDesc.addressModeV = wgpu::AddressMode::Repeat;
	samplerDesc.addressModeW = wgpu::AddressMode::Repeat;
	samplerDesc.magFilter = wgpu::FilterMode::Linear;
	samplerDesc.minFilter = wgpu::FilterMode::Linear;
	samplerDesc.mipmapFilter = wgpu::MipmapFilterMode::Linear;
	samplerDesc.lodMinClamp = 0.0f;
	samplerDesc.lodMaxClamp = 8.0f;
	samplerDesc.compare = wgpu::CompareFunction::Undefined;
	samplerDesc.maxAnisotropy = 1;
	sampler2 = device.CreateSampler(&samplerDesc);


	// Create a texture
	//wgpu::TextureDescriptor textureDesc;
	//textureDesc.dimension = wgpu::TextureDimension::e2D;
	//textureDesc.size = { 256, 256, 1 };
	//textureDesc.mipLevelCount = 1;
	//textureDesc.sampleCount = 1;
	//textureDesc.format = wgpu::TextureFormat::RGBA8Unorm;
	//textureDesc.usage = wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopyDst;
	//textureDesc.viewFormatCount = 0;
	//textureDesc.viewFormats = nullptr;
	// Create image data
	//std::vector<uint8_t> pixels(4 * textureDesc.size.width * textureDesc.size.height);
	//for (uint32_t i = 0; i < textureDesc.size.width; ++i) {
	//	for (uint32_t j = 0; j < textureDesc.size.height; ++j) {
	//		uint8_t* p = &pixels[4 * (j * textureDesc.size.width + i)];
	//		p[0] = (uint8_t)i; // r
	//		p[1] = (uint8_t)j; // g
	//		p[2] = 128; // b
	//		p[3] = 255; // a
	//	}
	//}
	// Create image data
	//std::vector<uint8_t> pixels(4 * textureDesc.size.width * textureDesc.size.height);
	//for (uint32_t i = 0; i < textureDesc.size.width; ++i) {
	//	for (uint32_t j = 0; j < textureDesc.size.height; ++j) {
	//		uint8_t* p = &pixels[4 * (j * textureDesc.size.width + i)];
	//		p[0] = (i / 16) % 2 == (j / 16) % 2 ? 255 : 0; // r
	//		p[1] = ((i - j) / 16) % 2 == 0 ? 255 : 0; // g
	//		p[2] = ((i + j) / 16) % 2 == 0 ? 255 : 0; // b
	//		p[3] = 255; // a
	//	}
	//}

	//texture2 = m_data->device.CreateTexture(&textureDesc);
	//wgpu::TextureViewDescriptor textureViewDesc;
	//textureViewDesc.aspect = wgpu::TextureAspect::All;
	//textureViewDesc.baseArrayLayer = 0;
	//textureViewDesc.arrayLayerCount = 1;
	//textureViewDesc.baseMipLevel = 0;
	//textureViewDesc.mipLevelCount = 1;
	//textureViewDesc.dimension = wgpu::TextureViewDimension::e2D;
	//textureViewDesc.format = textureDesc.format;
	//textureView = texture2.CreateView(&textureViewDesc);

	texture2 = LoadTexture(device, "../Data/Models/fourareen2K_albedo.jpg", &textureView);

	return textureView != nullptr;
}

bool initGeometry(wgpu::Device device)
{
	// Load mesh data from OBJ file
	bool success = loadGeometryFromObj("../Data/models/fourareen.obj", vertexData);
	if (!success)
	{
		Fatal("Could not load geometry!");
		return false;
	}

	vertexBuffer2 = CreateBuffer(device, vertexData.data(), vertexData.size() * sizeof(VertexAttributes), wgpu::BufferUsage::Vertex);
	indexCount = static_cast<int>(vertexData.size());

	return vertexBuffer2 != nullptr;
}

bool initUniforms(wgpu::Device device)
{
	// Update view matrix
	uniforms.modelMatrix = glm::mat4x4(1.0);
	uniforms.viewMatrix = glm::lookAt(glm::vec3(-2.0f, -3.0f, 2.0f), glm::vec3(0.0f), glm::vec3(0, 0, 1));
	uniforms.projectionMatrix = glm::perspective(45 * 3.14f / 180, 640.0f / 480.0f, 0.01f, 100.0f);
	uniforms.time = 1.0f;
	uniforms.color = { 0.0f, 1.0f, 0.4f, 1.0f };
	uniformBuffer = CreateBuffer(device, &uniforms, sizeof(MyUniforms), wgpu::BufferUsage::Uniform);

	return uniformBuffer != nullptr;
}

bool initBindGroup(wgpu::Device device) 
{
	// Create a binding
	std::vector<wgpu::BindGroupEntry> bindings(3);

	// The index of the binding (the entries in bindGroupDesc can be in any order)
	bindings[0].binding = 0;
	// The buffer it is actually bound to
	bindings[0].buffer = uniformBuffer;
	// We can specify an offset within the buffer, so that a single buffer can hold multiple uniform blocks.
	bindings[0].offset = 0;
	// And we specify again the size of the buffer.
	bindings[0].size = sizeof(MyUniforms);

	bindings[1].binding = 1;
	bindings[1].textureView = textureView;

	bindings[2].binding = 2;
	bindings[2].sampler = sampler2;

	// A bind group contains one or multiple bindings
	wgpu::BindGroupDescriptor bindGroupDesc{};
	bindGroupDesc.layout = bindGroupLayout;
	// There must be as many bindings as declared in the layout!
	bindGroupDesc.entryCount = (uint32_t)bindings.size();
	bindGroupDesc.entries = bindings.data();
	bindGroup2 = device.CreateBindGroup(&bindGroupDesc);

	return bindGroup2 != nullptr;
}

void initTextures(wgpu::Device device, wgpu::Queue queue)
{
	wgpu::TextureDescriptor descriptor;
	descriptor.dimension = wgpu::TextureDimension::e2D;
	descriptor.size.width = 1024;
	descriptor.size.height = 1024;
	descriptor.size.depthOrArrayLayers = 1;
	descriptor.sampleCount = 1;
	descriptor.format = wgpu::TextureFormat::RGBA8Unorm;
	descriptor.mipLevelCount = 1;
	descriptor.usage = wgpu::TextureUsage::CopyDst | wgpu::TextureUsage::TextureBinding;
	texture = device.CreateTexture(&descriptor);

	sampler = device.CreateSampler();

	// Initialize the texture with arbitrary data until we can load images
	std::vector<uint8_t> data(4 * 1024 * 1024, 0);
	for (size_t i = 0; i < data.size(); ++i) 
		data[i] = static_cast<uint8_t>(i % 253);

	wgpu::Buffer stagingBuffer = CreateBuffer(device, data.data(), static_cast<uint32_t>(data.size()), wgpu::BufferUsage::CopySrc);
	wgpu::ImageCopyBuffer imageCopyBuffer = CreateImageCopyBuffer(stagingBuffer, 0, 4 * 1024);
	wgpu::ImageCopyTexture imageCopyTexture = CreateImageCopyTexture(texture, 0, { 0, 0, 0 });
	wgpu::Extent3D copySize = { 1024, 1024, 1 };

	wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
	encoder.CopyBufferToTexture(&imageCopyBuffer, &imageCopyTexture, &copySize);

	wgpu::CommandBuffer copy = encoder.Finish();
	queue.Submit(1, &copy);
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
bool Render::Create(void* glfwWindow)
{
	m_data = new RenderData();

	if (!createDevice(glfwWindow))
		return false;

	if (!initSwapChain(kWidth, kHeight)) 
		return false;
	if (!initDepthBuffer(kWidth, kHeight))
		return false;

	if (!initRenderPipeline(m_data->device, m_data->swapChainFormat, m_data->depthTextureFormat))
		return false;
	if (!initTexture(m_data->device))
		return false;
	if (!initGeometry(m_data->device))
		return false;
	if (!initUniforms(m_data->device))
		return false;
	if (!initBindGroup(m_data->device))
		return false;

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
	if (updateDragInertia())
		updateViewMatrix();

	{
		// Update uniform buffer
		uniforms.time = static_cast<float>(glfwGetTime());
		m_data->queue.WriteBuffer(uniformBuffer, offsetof(MyUniforms, time), &uniforms.time, sizeof(MyUniforms::time));
	}
	
	wgpu::TextureView backbufferView = m_data->swapChain.GetCurrentTextureView();
	if (!backbufferView)
	{
		Fatal("Cannot acquire next swap chain texture");
		return;
	}

	wgpu::RenderPassColorAttachment renderPassColorAttachment{};
	renderPassColorAttachment.view = backbufferView;
	renderPassColorAttachment.resolveTarget = nullptr;
	renderPassColorAttachment.loadOp = wgpu::LoadOp::Clear;
	renderPassColorAttachment.storeOp = wgpu::StoreOp::Store;
	renderPassColorAttachment.clearValue = wgpu::Color{ 0.05f, 0.05f, 0.05f, 1.0f };

	wgpu::RenderPassDepthStencilAttachment depthStencilAttachment{};
	depthStencilAttachment.view = m_data->depthTextureView;
	// The initial value of the depth buffer, meaning "far"
	depthStencilAttachment.depthClearValue = 1.0f;
	// Operation settings comparable to the color attachment
	depthStencilAttachment.depthLoadOp = wgpu::LoadOp::Clear;
	depthStencilAttachment.depthStoreOp = wgpu::StoreOp::Store;
	// we could turn off writing to the depth buffer globally here
	depthStencilAttachment.depthReadOnly = false;
	// Stencil setup, mandatory but unused
	depthStencilAttachment.stencilClearValue = 0;
	//depthStencilAttachment.stencilLoadOp = wgpu::LoadOp::Clear;
	//depthStencilAttachment.stencilStoreOp = wgpu::StoreOp::Store;
	//depthStencilAttachment.stencilReadOnly = true;

	wgpu::RenderPassDescriptor renderPassDesc{};
	renderPassDesc.colorAttachmentCount = 1;
	renderPassDesc.colorAttachments = &renderPassColorAttachment;
	renderPassDesc.depthStencilAttachment = &depthStencilAttachment;
	renderPassDesc.timestampWrites = nullptr;

	wgpu::CommandEncoderDescriptor commandEncoderDesc{};
	wgpu::CommandEncoder encoder = m_data->device.CreateCommandEncoder(&commandEncoderDesc);
	{
		wgpu::RenderPassEncoder renderPass = encoder.BeginRenderPass(&renderPassDesc);
		renderPass.SetPipeline(pipeline2);
		renderPass.SetVertexBuffer(0, vertexBuffer2, 0, vertexData.size() * sizeof(VertexAttributes));
		//renderPass.SetIndexBuffer(indexBuffer2, wgpu::IndexFormat::Uint16, 0/*, indexData.size() * sizeof(uint16_t)*/);
		renderPass.SetBindGroup(0, bindGroup2, 0, nullptr);
		renderPass.Draw(indexCount, 1, 0, 0);
		renderPass.End();
	}

#if 1
	wgpu::CommandBuffer commands = encoder.Finish();
	m_data->queue.Submit(1, &commands);
#else
	// это пример отправки сразу нескольких командных буферов
	std::array<wgpu::CommandBuffer, 3> commands;
	m_data->queue.Submit(commands.size(), commands.data());
#endif

	m_data->swapChain.Present();
	// Check for pending error callbacks
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
	terminateDepthBuffer();
	terminateSwapChain();

	if (!initSwapChain(width, height)) return false;
	if (!initDepthBuffer(width, height)) return false;

	updateProjectionMatrix(width, height);

	return true;
}
//-----------------------------------------------------------------------------
void Render::OnMouseMove(double xpos, double ypos)
{
	if (m_drag.active)
	{
		glm::vec2 currentMouse = glm::vec2(-(float)xpos, (float)ypos);
		glm::vec2 delta = (currentMouse - m_drag.startMouse) * m_drag.sensitivity;
		m_cameraState.angles = m_drag.startCameraState.angles + delta;
		// Clamp to avoid going too far when orbitting up/down
		m_cameraState.angles.y = glm::clamp(m_cameraState.angles.y, -3.14f / 2 + 1e-5f, 3.14f / 2 - 1e-5f);
		updateViewMatrix();

		// Inertia
		m_drag.velocity = delta - m_drag.previousDelta;
		m_drag.previousDelta = delta;
	}
}
//-----------------------------------------------------------------------------
void Render::OnMouseButton(int button, int action, int mods, double xpos, double ypos)
{
	if (button == GLFW_MOUSE_BUTTON_LEFT)
	{
		switch (action)
		{
		case GLFW_PRESS:
			m_drag.active = true;
			m_drag.startMouse = glm::vec2(-(float)xpos, (float)ypos);
			m_drag.startCameraState = m_cameraState;
			break;
		case GLFW_RELEASE:
			m_drag.active = false;
			break;
		}
	}
}
//-----------------------------------------------------------------------------
void Render::OnScroll(double xoffset, double yoffset)
{
	m_cameraState.zoom += m_drag.scrollSensitivity * static_cast<float>(yoffset);
	m_cameraState.zoom = glm::clamp(m_cameraState.zoom, -2.0f, 2.0f);
	updateViewMatrix();
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

	// Create the view of the depth texture manipulated by the rasterizer
	wgpu::TextureViewDescriptor depthTextureViewDescriptor{};
	depthTextureViewDescriptor.aspect = wgpu::TextureAspect::DepthOnly;
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
void Render::updateProjectionMatrix(int width, int height)
{
	// Update projection matrix
	float ratio = width / (float)height;
	uniforms.projectionMatrix = glm::perspective(45 * 3.14f / 180, ratio, 0.01f, 100.0f);
	m_data->queue.WriteBuffer(uniformBuffer, offsetof(MyUniforms, projectionMatrix), &uniforms.projectionMatrix, sizeof(MyUniforms::projectionMatrix));
}
//-----------------------------------------------------------------------------
void Render::updateViewMatrix()
{
	float cx = cos(m_cameraState.angles.x);
	float sx = sin(m_cameraState.angles.x);
	float cy = cos(m_cameraState.angles.y);
	float sy = sin(m_cameraState.angles.y);
	glm::vec3 position = glm::vec3(cx * cy, sx * cy, sy) * std::exp(-m_cameraState.zoom);
	uniforms.viewMatrix = glm::lookAt(position, glm::vec3(0.0f), glm::vec3(0, 0, 1));
	m_data->queue.WriteBuffer(
		uniformBuffer,
		offsetof(MyUniforms, viewMatrix),
		&uniforms.viewMatrix,
		sizeof(MyUniforms::viewMatrix)
	);
}
//-----------------------------------------------------------------------------
