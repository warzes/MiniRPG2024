#include "Engine.h"
#include "RenderUtils.h"


void PrintDeviceError(WGPUErrorType errorType, const char* message, void*) {
	const char* errorTypeName = "";
	switch (errorType) {
	case WGPUErrorType_Validation:
		errorTypeName = "Validation";
		break;
	case WGPUErrorType_OutOfMemory:
		errorTypeName = "Out of memory";
		break;
	case WGPUErrorType_Unknown:
		errorTypeName = "Unknown";
		break;
	case WGPUErrorType_DeviceLost:
		errorTypeName = "Device lost";
		break;
	default:
		//DAWN_UNREACHABLE();
		return;
	}
	//dawn::ErrorLog() << errorTypeName << " error: " << message;
}

void DeviceLostCallback(WGPUDeviceLostReason reason, const char* message, void*)
{
	//dawn::ErrorLog() << "Device lost: " << message;
}

void DeviceLogCallback(WGPULoggingType type, const char* message, void*)
{
	//dawn::ErrorLog() << "Device log: " << message;
}

wgpu::AdapterType adapterType = wgpu::AdapterType::Unknown;
constexpr uint32_t kWidth = 1024;
constexpr uint32_t kHeight = 768;

wgpu::Buffer indexBuffer;
wgpu::Buffer vertexBuffer;
wgpu::Texture texture;
wgpu::Sampler sampler;
wgpu::RenderPipeline pipeline;
wgpu::BindGroup bindGroup;

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
	for (size_t i = 0; i < data.size(); ++i) {
		data[i] = static_cast<uint8_t>(i % 253);
	}

	wgpu::Buffer stagingBuffer = CreateBuffer(device, data.data(), static_cast<uint32_t>(data.size()), wgpu::BufferUsage::CopySrc);
	wgpu::ImageCopyBuffer imageCopyBuffer = CreateImageCopyBuffer(stagingBuffer, 0, 4 * 1024);
	wgpu::ImageCopyTexture imageCopyTexture = CreateImageCopyTexture(texture, 0, { 0, 0, 0 });
	wgpu::Extent3D copySize = { 1024, 1024, 1 };

	wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
	encoder.CopyBufferToTexture(&imageCopyBuffer, &imageCopyTexture, &copySize);

	wgpu::CommandBuffer copy = encoder.Finish();
	queue.Submit(1, &copy);
}

struct RenderData
{
	std::unique_ptr<dawn::native::Instance> instance;
	wgpu::Device device;
	wgpu::Queue queue;
	wgpu::SwapChain swapChain;
	wgpu::TextureView depthStencilView;
};

bool Render::Create(void* glfwWindow)
{
	m_data = new RenderData();

	if (!createDevice(glfwWindow))
		return false;

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

	return true;
}

void Render::Destroy()
{
	m_data->instance.reset();
	delete m_data;
}

void Render::Update()
{
	dawn::native::InstanceProcessEvents(m_data->instance->Get());
}

void Render::Frame()
{
	wgpu::TextureView backbufferView = m_data->swapChain.GetCurrentTextureView();
	if (!backbufferView)
	{
		puts("Cannot acquire next swap chain texture");
		return;
	}

	wgpu::RenderPassColorAttachment renderPassColorAttachment = {};
	renderPassColorAttachment.view = backbufferView;
	renderPassColorAttachment.loadOp = wgpu::LoadOp::Clear;
	renderPassColorAttachment.storeOp = wgpu::StoreOp::Store;
	renderPassColorAttachment.clearValue = wgpu::Color{ 0.95f, 0.35f, 0.49f, 1.0f };

	//dawn::utils::ComboRenderPassDescriptor renderPassDesc({ backbufferView }, depthStencilView);
	dawn::utils::ComboRenderPassDescriptor renderPassDesc{};
	renderPassDesc.colorAttachmentCount = 1;
	renderPassDesc.colorAttachments = &renderPassColorAttachment;
	renderPassDesc.depthStencilAttachment = nullptr;

	wgpu::RenderPassDepthStencilAttachment cDepthStencilAttachmentInfo = {};
	cDepthStencilAttachmentInfo.depthClearValue = 1.0f;
	cDepthStencilAttachmentInfo.stencilClearValue = 0;
	cDepthStencilAttachmentInfo.depthLoadOp = wgpu::LoadOp::Clear;
	cDepthStencilAttachmentInfo.depthStoreOp = wgpu::StoreOp::Store;
	cDepthStencilAttachmentInfo.stencilLoadOp = wgpu::LoadOp::Clear;
	cDepthStencilAttachmentInfo.stencilStoreOp = wgpu::StoreOp::Store;
	if (m_data->depthStencilView.Get() != nullptr) 
	{
		cDepthStencilAttachmentInfo.view = m_data->depthStencilView;
		renderPassDesc.depthStencilAttachment = &cDepthStencilAttachmentInfo;
	}

	wgpu::CommandEncoder encoder = m_data->device.CreateCommandEncoder();
	{
		wgpu::RenderPassEncoder renderPass = encoder.BeginRenderPass(&renderPassDesc);
		renderPass.SetPipeline(pipeline);
		renderPass.SetBindGroup(0, bindGroup);
		renderPass.SetVertexBuffer(0, vertexBuffer);
		renderPass.SetIndexBuffer(indexBuffer, wgpu::IndexFormat::Uint32);
		renderPass.DrawIndexed(3);
		renderPass.End();
	}

	wgpu::CommandBuffer commands = encoder.Finish();
	m_data->queue.Submit(1, &commands);

	m_data->swapChain.Present();
}

std::optional<dawn::native::Adapter> requestAdapter(dawn::native::Instance* instance);
wgpu::TextureView createDefaultDepthStencilView(const wgpu::Device& device);

bool Render::createDevice(void* glfwWindow)
{
	// 1. We create a descriptor
	WGPUInstanceDescriptor instanceDescriptor{};
	instanceDescriptor.features.timedWaitAnyEnable = true;
	// 2. We create the instance using this descriptor
	m_data->instance = std::make_unique<dawn::native::Instance>(&instanceDescriptor);
	// 3. We can check whether there is actually an instance created
	if (!m_data->instance)
	{
		puts("Could not initialize WebGPU!");
		return false;
	}

	// 4. Request Adapter
	auto preferredAdapter = requestAdapter(m_data->instance.get());
	if (!preferredAdapter) return false;

	WGPUDawnTogglesDescriptor toggles{};
	toggles.chain.sType = WGPUSType_DawnTogglesDescriptor;
	WGPUDeviceDescriptor deviceDesc{};
	deviceDesc.nextInChain = reinterpret_cast<WGPUChainedStruct*>(&toggles);

	WGPUDevice backendDevice = preferredAdapter->CreateDevice(&deviceDesc);
	DawnProcTable backendProcs = dawn::native::GetProcs();

	// Create the swapchain
	auto surfaceChainedDesc = wgpu::glfw::SetupWindowAndGetSurfaceDescriptor((GLFWwindow*)glfwWindow);
	WGPUSurfaceDescriptor surfaceDesc;
	surfaceDesc.nextInChain = reinterpret_cast<WGPUChainedStruct*>(surfaceChainedDesc.get());
	WGPUSurface surface = backendProcs.instanceCreateSurface(m_data->instance->Get(), &surfaceDesc);

	WGPUSwapChainDescriptor swapChainDesc = {};
	swapChainDesc.usage = WGPUTextureUsage_RenderAttachment;
	swapChainDesc.format = static_cast<WGPUTextureFormat>(wgpu::TextureFormat::BGRA8Unorm);
	swapChainDesc.width = kWidth;
	swapChainDesc.height = kHeight;
	swapChainDesc.presentMode = WGPUPresentMode_Mailbox;
	WGPUSwapChain backendSwapChain = backendProcs.deviceCreateSwapChain(backendDevice, surface, &swapChainDesc);

	// Choose whether to use the backend procs and devices/swapchains directly, or set up the wire.
	WGPUDevice cDevice = nullptr;
	DawnProcTable procs;

	procs = backendProcs;
	cDevice = backendDevice;
	m_data->swapChain = wgpu::SwapChain::Acquire(backendSwapChain);

	dawnProcSetProcs(&procs);
	procs.deviceSetUncapturedErrorCallback(cDevice, PrintDeviceError, nullptr);
	procs.deviceSetDeviceLostCallback(cDevice, DeviceLostCallback, nullptr);
	procs.deviceSetLoggingCallback(cDevice, DeviceLogCallback, nullptr);

	m_data->device = wgpu::Device::Acquire(cDevice);

	m_data->queue = m_data->device.GetQueue();

	m_data->depthStencilView = createDefaultDepthStencilView(m_data->device);

	return true;
}

std::optional<dawn::native::Adapter> requestAdapter(dawn::native::Instance* instance)
{
	wgpu::RequestAdapterOptions options{};
	options.backendType = wgpu::BackendType::D3D12;

	// Get an adapter for the backend to use, and create the device.
	auto adapters = instance->EnumerateAdapters(&options);

	wgpu::DawnAdapterPropertiesPowerPreference powerProps{};
	wgpu::AdapterProperties adapterProperties{};
	adapterProperties.nextInChain = &powerProps;
	// Find the first adapter which satisfies the adapterType requirement.
	auto isAdapterType = [&adapterProperties](const auto& adapter) -> bool {
			// picks the first adapter when adapterType is unknown.
			if (adapterType == wgpu::AdapterType::Unknown)
				return true;
			adapter.GetProperties(&adapterProperties);
			return adapterProperties.adapterType == adapterType;
		};
	auto preferredAdapter = std::find_if(adapters.begin(), adapters.end(), isAdapterType);
	if (preferredAdapter == adapters.end())
	{
		puts("Failed to find an adapter! Please try another adapter type.");
		return std::nullopt;
	}

	return *preferredAdapter;
}

wgpu::TextureView createDefaultDepthStencilView(const wgpu::Device& device)
{
	wgpu::TextureDescriptor descriptor{};
	descriptor.dimension = wgpu::TextureDimension::e2D;
	descriptor.size.width = kWidth;
	descriptor.size.height = kHeight;
	descriptor.size.depthOrArrayLayers = 1;
	descriptor.sampleCount = 1;
	descriptor.format = wgpu::TextureFormat::Depth24PlusStencil8;
	descriptor.mipLevelCount = 1;
	descriptor.usage = wgpu::TextureUsage::RenderAttachment;
	auto depthStencilTexture = device.CreateTexture(&descriptor);
	return depthStencilTexture.CreateView();
}