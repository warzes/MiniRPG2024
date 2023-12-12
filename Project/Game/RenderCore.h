#pragma once

//-----------------------------------------------------------------------------
void wgpuPrintDeviceError(WGPUErrorType errorType, const char* message, void*) noexcept
{
	std::string errorTypeName = "";
	switch (errorType)
	{
	case WGPUErrorType_Validation:  errorTypeName = "Validation"; break;
	case WGPUErrorType_OutOfMemory: errorTypeName = "Out of memory"; break;
	case WGPUErrorType_Internal:    errorTypeName = "Internal"; break;
	case WGPUErrorType_Unknown:     errorTypeName = "Unknown"; break;
	case WGPUErrorType_DeviceLost:  errorTypeName = "Device lost"; break;
	}
	Fatal(errorTypeName + " - " + std::string(message));
}
//-----------------------------------------------------------------------------
void wgpuDeviceLostCallback(WGPUDeviceLostReason reason, const char* message, void*) noexcept
{
	Fatal("Device lost: " + std::string(message));
}
//-----------------------------------------------------------------------------
void wgpuDeviceLogCallback(WGPULoggingType type, const char* message, void*) noexcept
{
	Error("Device log: " + std::string(message));
}
//-----------------------------------------------------------------------------
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
//-----------------------------------------------------------------------------
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
//-----------------------------------------------------------------------------