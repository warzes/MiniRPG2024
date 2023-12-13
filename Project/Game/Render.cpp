#include "Engine.h"

#include <Dawn/webgpu.h>
#include <dawn/webgpu_cpp.h>
#include <Dawn/native/DawnNative.h>
#include <webgpu/webgpu_glfw.h>
#include <Dawn/utils/WGPUHelpers.h>
#include <Dawn/utils/ComboRenderPipelineDescriptor.h>
#include <Dawn/dawn_proc.h>

#include "RenderUtils.h"
#include "RenderCore.h"
#include "RenderResources.h"
//-----------------------------------------------------------------------------
constexpr uint32_t kWidth = 1024;
constexpr uint32_t kHeight = 768;

wgpu::Buffer indexBuffer;
wgpu::Buffer vertexBuffer;
wgpu::Texture texture;
wgpu::Sampler sampler;
wgpu::RenderPipeline pipeline;
wgpu::BindGroup bindGroup;

wgpu::RenderPipeline pipeline2;

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
	std::unique_ptr<dawn::native::Instance> dawnInstance;

	wgpu::Device device;
	wgpu::Queue queue;
	wgpu::SwapChain swapChain;
	wgpu::TextureView depthStencilView;
};
//-----------------------------------------------------------------------------
bool Render::Create(void* glfwWindow)
{
	m_data = new RenderData();

	if (!createDevice(glfwWindow))
		return false;

	m_data->depthStencilView = createDefaultDepthStencilView(m_data->device, kWidth, kHeight);

	constexpr uint32_t indexData[] = { 0, 1, 2 };
	indexBuffer = CreateBuffer(m_data->device, indexData, sizeof(indexData), wgpu::BufferUsage::Index);

	constexpr float vertexData[] = { 0.0f, 0.5f, 0.0f, 1.0f, -0.5f, -0.5f, 0.0f, 1.0f, 0.5f, -0.5f, 0.0f, 1.0f };
	vertexBuffer = CreateBuffer(m_data->device, vertexData, sizeof(vertexData), wgpu::BufferUsage::Vertex);

	initTextures(m_data->device, m_data->queue);

	wgpu::ShaderModule vsModule = CreateShaderModule(m_data->device, R"(
        @vertex fn main(@location(0) pos : vec4f)
                            -> @builtin(position) vec4f {
            return pos;
        })");

	wgpu::ShaderModule fsModule = CreateShaderModule(m_data->device, R"(
        @group(0) @binding(0) var mySampler: sampler;
        @group(0) @binding(1) var myTexture : texture_2d<f32>;

        @fragment fn main(@builtin(position) FragCoord : vec4f)
                              -> @location(0) vec4f {
            return textureSample(myTexture, mySampler, FragCoord.xy / vec2f(1024.0, 768.0));
        })");

	auto bgl = dawn::utils::MakeBindGroupLayout(
		m_data->device, {
					{0, wgpu::ShaderStage::Fragment, wgpu::SamplerBindingType::Filtering},
					{1, wgpu::ShaderStage::Fragment, wgpu::TextureSampleType::Float},
		});

	dawn::utils::ComboRenderPipelineDescriptor pipelineDescriptor{};
	pipelineDescriptor.layout = dawn::utils::MakeBasicPipelineLayout(m_data->device, &bgl);
	pipelineDescriptor.vertex.module = vsModule;
	pipelineDescriptor.vertex.bufferCount = 1;
	pipelineDescriptor.cBuffers[0].arrayStride = 4 * sizeof(float);
	pipelineDescriptor.cBuffers[0].attributeCount = 1;
	pipelineDescriptor.cAttributes[0].format = wgpu::VertexFormat::Float32x4;
	pipelineDescriptor.cFragment.module = fsModule;
	pipelineDescriptor.cTargets[0].format = wgpu::TextureFormat::BGRA8Unorm;
	pipelineDescriptor.EnableDepthStencil(wgpu::TextureFormat::Depth24PlusStencil8);

	pipeline = m_data->device.CreateRenderPipeline(&pipelineDescriptor);

	wgpu::TextureView view = texture.CreateView();
	bindGroup = dawn::utils::MakeBindGroup(m_data->device, bgl, { {0, sampler}, {1, view} });












	wgpu::ShaderModule vsModule2 = CreateShaderModule(m_data->device, 
R"(
@vertex
fn vs_main(@builtin(vertex_index) in_vertex_index: u32) -> @builtin(position) vec4f
{
	var p = vec2f(0.0, 0.0);
	if (in_vertex_index == 0u) {
		p = vec2f(-0.5, -0.5);
	} else if (in_vertex_index == 1u) {
		p = vec2f(0.5, -0.5);
	} else {
		p = vec2f(0.0, 0.5);
	}
	return vec4f(p, 0.0, 1.0);
})");

	wgpu::ShaderModule fsModule2 = CreateShaderModule(m_data->device, 
R"(
@fragment
fn fs_main() -> @location(0) vec4f
{
	return vec4f(0.0, 0.4, 1.0, 1.0);
})");


	wgpu::BlendState blendState{};
	blendState.color.srcFactor = wgpu::BlendFactor::SrcAlpha;
	blendState.color.dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha;
	blendState.color.operation = wgpu::BlendOperation::Add;
	blendState.alpha.srcFactor = wgpu::BlendFactor::Zero;
	blendState.alpha.dstFactor = wgpu::BlendFactor::One;
	blendState.alpha.operation = wgpu::BlendOperation::Add;
	wgpu::ColorTargetState colorTarget{};
	colorTarget.format = wgpu::TextureFormat::BGRA8Unorm;
	colorTarget.blend = &blendState;
	colorTarget.writeMask = wgpu::ColorWriteMask::All;

	wgpu::FragmentState fragmentState{};
	fragmentState.module = fsModule2;
	fragmentState.entryPoint = "fs_main";
	fragmentState.constantCount = 0;
	fragmentState.constants = nullptr;
	fragmentState.targetCount = 1;
	fragmentState.targets = &colorTarget;

	wgpu::RenderPipelineDescriptor pipelineDesc{};
	pipelineDesc.vertex.bufferCount = 0;
	pipelineDesc.vertex.buffers = nullptr;
	pipelineDesc.vertex.module = vsModule2;
	pipelineDesc.vertex.entryPoint = "vs_main";
	pipelineDesc.vertex.constantCount = 0;
	pipelineDesc.vertex.constants = nullptr;
	pipelineDesc.primitive.topology = wgpu::PrimitiveTopology::TriangleList;
	pipelineDesc.primitive.stripIndexFormat = wgpu::IndexFormat::Undefined;
	pipelineDesc.primitive.frontFace = wgpu::FrontFace::CCW;
	pipelineDesc.primitive.cullMode = wgpu::CullMode::None;
	pipelineDesc.fragment = &fragmentState;
	pipelineDesc.depthStencil = nullptr;
	pipelineDesc.multisample.count = 1;
	pipelineDesc.multisample.mask = ~0u;
	pipelineDesc.multisample.alphaToCoverageEnabled = false;
	pipelineDesc.layout = nullptr;

	pipeline2 = m_data->device.CreateRenderPipeline(&pipelineDesc);

	return true;
}
//-----------------------------------------------------------------------------
void Render::Destroy()
{
	wgpuSwapChainRelease(m_data->swapChain.Get());
	m_data->dawnInstance.reset();
	delete m_data;
}
//-----------------------------------------------------------------------------
void Render::Frame()
{
	dawn::native::InstanceProcessEvents(m_data->dawnInstance->Get());

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
	renderPassColorAttachment.clearValue = wgpu::Color{ 0.95f, 0.35f, 0.49f, 1.0f };

	//wgpu::RenderPassDepthStencilAttachment cDepthStencilAttachmentInfo{};
	//cDepthStencilAttachmentInfo.depthClearValue = 1.0f;
	//cDepthStencilAttachmentInfo.stencilClearValue = 0;
	//cDepthStencilAttachmentInfo.depthLoadOp = wgpu::LoadOp::Clear;
	//cDepthStencilAttachmentInfo.depthStoreOp = wgpu::StoreOp::Store;
	//cDepthStencilAttachmentInfo.stencilLoadOp = wgpu::LoadOp::Clear;
	//cDepthStencilAttachmentInfo.stencilStoreOp = wgpu::StoreOp::Store;
	//cDepthStencilAttachmentInfo.view = m_data->depthStencilView;

	wgpu::RenderPassDescriptor renderPassDesc{};
	renderPassDesc.colorAttachmentCount = 1;
	renderPassDesc.colorAttachments = &renderPassColorAttachment;
	//renderPassDesc.depthStencilAttachment = &cDepthStencilAttachmentInfo;
	renderPassDesc.depthStencilAttachment = nullptr;

	wgpu::CommandEncoder encoder = m_data->device.CreateCommandEncoder();
	{
		wgpu::RenderPassEncoder renderPass = encoder.BeginRenderPass(&renderPassDesc);
		renderPass.SetPipeline(pipeline2);
		//renderPass.SetBindGroup(0, bindGroup);
		//renderPass.SetVertexBuffer(0, vertexBuffer);
		//renderPass.SetIndexBuffer(indexBuffer, wgpu::IndexFormat::Uint32);
		//renderPass.DrawIndexed(3);
		renderPass.Draw(3, 1, 0, 0);
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
}
//-----------------------------------------------------------------------------
bool Render::createDevice(void* glfwWindow)
{
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

	WGPUSwapChainDescriptor swapChainDesc = {};
	swapChainDesc.usage = WGPUTextureUsage_RenderAttachment;
	swapChainDesc.format = static_cast<WGPUTextureFormat>(wgpu::TextureFormat::BGRA8Unorm);
	swapChainDesc.width = kWidth;
	swapChainDesc.height = kHeight;
	swapChainDesc.presentMode = WGPUPresentMode_Mailbox;
	WGPUSwapChain backendSwapChain = wgpuDeviceCreateSwapChain(backendDevice, surface, &swapChainDesc);

	m_data->swapChain = wgpu::SwapChain::Acquire(backendSwapChain);
	m_data->device = wgpu::Device::Acquire(backendDevice);
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