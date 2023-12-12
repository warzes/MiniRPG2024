#include "Engine.h"

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

std::unique_ptr<dawn::native::Instance> instance;
wgpu::SwapChain swapChain;
wgpu::AdapterType adapterType = wgpu::AdapterType::Unknown;
static dawn::wire::WireServer* wireServer = nullptr;
static dawn::wire::WireClient* wireClient = nullptr;
static dawn::utils::TerribleCommandBuffer* c2sBuf = nullptr;
static dawn::utils::TerribleCommandBuffer* s2cBuf = nullptr;

enum class CmdBufType {
	None,
	Terrible,
};
static CmdBufType cmdBufType = CmdBufType::Terrible;

wgpu::TextureFormat GetPreferredSwapChainTextureFormat() {
	return wgpu::TextureFormat::BGRA8Unorm;
}

constexpr uint32_t kWidth = 1024;
constexpr uint32_t kHeight = 768;


//wgpu::Device device;

wgpu::Buffer indexBuffer;
wgpu::Buffer vertexBuffer;

wgpu::Texture texture;
wgpu::Sampler sampler;

//wgpu::Queue queue;
wgpu::TextureView depthStencilView;
wgpu::RenderPipeline pipeline;
wgpu::BindGroup bindGroup;


void initBuffers(wgpu::Device device)
{
	static const uint32_t indexData[3] = {
		0,
		1,
		2,
	};
	indexBuffer = dawn::utils::CreateBufferFromData(device, indexData, sizeof(indexData),
		wgpu::BufferUsage::Index);

	static const float vertexData[12] = {
		0.0f, 0.5f, 0.0f, 1.0f, -0.5f, -0.5f, 0.0f, 1.0f, 0.5f, -0.5f, 0.0f, 1.0f,
	};
	vertexBuffer = dawn::utils::CreateBufferFromData(device, vertexData, sizeof(vertexData),
		wgpu::BufferUsage::Vertex);
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
	for (size_t i = 0; i < data.size(); ++i) {
		data[i] = static_cast<uint8_t>(i % 253);
	}

	wgpu::Buffer stagingBuffer = dawn::utils::CreateBufferFromData(
		device, data.data(), static_cast<uint32_t>(data.size()), wgpu::BufferUsage::CopySrc);
	wgpu::ImageCopyBuffer imageCopyBuffer =
		dawn::utils::CreateImageCopyBuffer(stagingBuffer, 0, 4 * 1024);
	wgpu::ImageCopyTexture imageCopyTexture =
		dawn::utils::CreateImageCopyTexture(texture, 0, { 0, 0, 0 });
	wgpu::Extent3D copySize = { 1024, 1024, 1 };

	wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
	encoder.CopyBufferToTexture(&imageCopyBuffer, &imageCopyTexture, &copySize);

	wgpu::CommandBuffer copy = encoder.Finish();
	queue.Submit(1, &copy);
}

wgpu::TextureView CreateDefaultDepthStencilView(const wgpu::Device& device) {
	wgpu::TextureDescriptor descriptor;
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

struct RenderData
{
	std::unique_ptr<dawn::native::Instance> instance;
	wgpu::Device device;
	wgpu::Queue queue;
	wgpu::SwapChain swapChain;
};

bool Render::Create(void* glfwWindow)
{
	m_data = new RenderData();

	if (!createDevice(glfwWindow))
		return false;

	initBuffers(m_data->device);
	initTextures(m_data->device, m_data->queue);

	wgpu::ShaderModule vsModule = dawn::utils::CreateShaderModule(m_data->device, R"(
        @vertex fn main(@location(0) pos : vec4f)
                            -> @builtin(position) vec4f {
            return pos;
        })");

	wgpu::ShaderModule fsModule = dawn::utils::CreateShaderModule(m_data->device, R"(
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

	wgpu::PipelineLayout pl = dawn::utils::MakeBasicPipelineLayout(m_data->device, &bgl);

	depthStencilView = CreateDefaultDepthStencilView(m_data->device);

	dawn::utils::ComboRenderPipelineDescriptor descriptor;
	descriptor.layout = dawn::utils::MakeBasicPipelineLayout(m_data->device, &bgl);
	descriptor.vertex.module = vsModule;
	descriptor.vertex.bufferCount = 1;
	descriptor.cBuffers[0].arrayStride = 4 * sizeof(float);
	descriptor.cBuffers[0].attributeCount = 1;
	descriptor.cAttributes[0].format = wgpu::VertexFormat::Float32x4;
	descriptor.cFragment.module = fsModule;
	descriptor.cTargets[0].format = GetPreferredSwapChainTextureFormat();
	descriptor.EnableDepthStencil(wgpu::TextureFormat::Depth24PlusStencil8);

	pipeline = m_data->device.CreateRenderPipeline(&descriptor);

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

	dawn::utils::ComboRenderPassDescriptor renderPassDesc({ backbufferView }, depthStencilView);
	dawn::utils::ComboRenderPassDescriptor renderPassDesc{};
	renderPassDesc.colorAttachmentCount = 1;
	renderPassDesc.colorAttachments = &renderPassColorAttachment;
	renderPassDesc.depthStencilAttachment = nullptr;

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
	
	if (cmdBufType == CmdBufType::Terrible) 
	{
		bool c2sSuccess = c2sBuf->Flush();
		bool s2cSuccess = s2cBuf->Flush();

		DAWN_ASSERT(c2sSuccess && s2cSuccess);
	}
}

std::optional<dawn::native::Adapter> requestAdapter(dawn::native::Instance* instance);

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

	switch (cmdBufType) 
	{
	case CmdBufType::None:
		procs = backendProcs;
		cDevice = backendDevice;
		m_data->swapChain = wgpu::SwapChain::Acquire(backendSwapChain);
		break;

	case CmdBufType::Terrible: 
	{
		c2sBuf = new dawn::utils::TerribleCommandBuffer();
		s2cBuf = new dawn::utils::TerribleCommandBuffer();

		dawn::wire::WireServerDescriptor serverDesc = {};
		serverDesc.procs = &backendProcs;
		serverDesc.serializer = s2cBuf;

		wireServer = new dawn::wire::WireServer(serverDesc);
		c2sBuf->SetHandler(wireServer);

		dawn::wire::WireClientDescriptor clientDesc = {};
		clientDesc.serializer = c2sBuf;

		wireClient = new dawn::wire::WireClient(clientDesc);
		procs = dawn::wire::client::GetProcs();
		s2cBuf->SetHandler(wireClient);

		auto deviceReservation = wireClient->ReserveDevice();
		wireServer->InjectDevice(backendDevice, deviceReservation.id,
			deviceReservation.generation);
		cDevice = deviceReservation.device;

		auto swapChainReservation = wireClient->ReserveSwapChain(cDevice, &swapChainDesc);
		wireServer->InjectSwapChain(backendSwapChain, swapChainReservation.id,
			swapChainReservation.generation, deviceReservation.id,
			deviceReservation.generation);
		m_data->swapChain = wgpu::SwapChain::Acquire(swapChainReservation.swapchain);
	} break;
	}

	dawnProcSetProcs(&procs);
	procs.deviceSetUncapturedErrorCallback(cDevice, PrintDeviceError, nullptr);
	procs.deviceSetDeviceLostCallback(cDevice, DeviceLostCallback, nullptr);
	procs.deviceSetLoggingCallback(cDevice, DeviceLogCallback, nullptr);

	m_data->device = wgpu::Device::Acquire(cDevice);

	m_data->queue = m_data->device.GetQueue();

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