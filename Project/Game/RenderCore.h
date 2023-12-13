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
wgpu::TextureView createDefaultDepthStencilView(const wgpu::Device& device, uint32_t width, uint32_t height)
{
	wgpu::TextureDescriptor descriptor{};
	descriptor.dimension = wgpu::TextureDimension::e2D;
	descriptor.size.width = width;
	descriptor.size.height = height;
	descriptor.size.depthOrArrayLayers = 1;
	descriptor.sampleCount = 1;
	descriptor.format = wgpu::TextureFormat::Depth24PlusStencil8;
	descriptor.mipLevelCount = 1;
	descriptor.usage = wgpu::TextureUsage::RenderAttachment;
	auto depthStencilTexture = device.CreateTexture(&descriptor);
	return depthStencilTexture.CreateView();
}
//-----------------------------------------------------------------------------