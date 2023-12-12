#pragma once

#include <Dawn/webgpu.h>
#include <Dawn/native/DawnNative.h>
#include <webgpu/webgpu_glfw.h>
#include <Dawn/utils/WGPUHelpers.h>
#include <Dawn/utils/ComboRenderPipelineDescriptor.h>
#include <Dawn/dawn_proc.h>

#include <stb/stb_image.h>
#define STBIR_DEFAULT_FILTER_DOWNSAMPLE STBIR_FILTER_BOX
#include <stb/stb_image_resize2.h>

wgpu::ImageCopyBuffer CreateImageCopyBuffer(wgpu::Buffer buffer, uint64_t offset = 0, uint32_t bytesPerRow = wgpu::kCopyStrideUndefined, uint32_t rowsPerImage = wgpu::kCopyStrideUndefined);
wgpu::ImageCopyTexture CreateImageCopyTexture(wgpu::Texture texture, uint32_t level = 0, wgpu::Origin3D origin = { 0, 0, 0 }, wgpu::TextureAspect aspect = wgpu::TextureAspect::All);
wgpu::TextureDataLayout CreateTextureDataLayout(uint64_t offset, uint32_t bytesPerRow, uint32_t rowsPerImage = wgpu::kCopyStrideUndefined);


wgpu::ShaderModule CreateShaderModule(const wgpu::Device& device, const char* source)
{
	wgpu::ShaderModuleWGSLDescriptor wgslDescriptor{};
	wgslDescriptor.code = source;
	wgpu::ShaderModuleDescriptor descriptor;
	descriptor.nextInChain = &wgslDescriptor;
	return device.CreateShaderModule(&descriptor);
}

wgpu::ShaderModule CreateShaderModule(const wgpu::Device& device, const std::string& source)
{
	return CreateShaderModule(device, source.c_str());
}

wgpu::Buffer CreateBuffer(const wgpu::Device& device, const void* data, uint64_t size, wgpu::BufferUsage usage)
{
	const wgpu::BufferDescriptor descriptor{
		.usage = usage | wgpu::BufferUsage::CopyDst,
		.size = size
	};
	wgpu::Buffer buffer = device.CreateBuffer(&descriptor);

	if (data) device.GetQueue().WriteBuffer(buffer, 0, data, size);
	return buffer;
}

template <typename T>
wgpu::Buffer CreateBuffer(const wgpu::Device& device, wgpu::BufferUsage usage, std::initializer_list<T> data) 
{
	return CreateBuffer(device, data.begin(), uint32_t(sizeof(T) * data.size()), usage);
}

wgpu::Buffer CreateVertexBuffer(wgpu::Device& device, size_t size, const void* data)
{
	return CreateBuffer(device, data, size, wgpu::BufferUsage::Vertex);
}

wgpu::Buffer CreateIndexBuffer(wgpu::Device& device, size_t size, const void* data)
{
	return CreateBuffer(device, data, size, wgpu::BufferUsage::Index);
}

wgpu::Buffer CreateUniformBuffer(wgpu::Device& device, size_t size, const void* data)
{
	return CreateBuffer(device, data, size, wgpu::BufferUsage::Uniform);
}

wgpu::TextureDataLayout CreateTextureDataLayout(uint64_t offset, uint32_t bytesPerRow, uint32_t rowsPerImage)
{
	wgpu::TextureDataLayout textureDataLayout{};
	textureDataLayout.offset = offset;
	textureDataLayout.bytesPerRow = bytesPerRow;
	textureDataLayout.rowsPerImage = rowsPerImage;

	return textureDataLayout;
}

wgpu::ImageCopyBuffer CreateImageCopyBuffer(wgpu::Buffer buffer, uint64_t offset, uint32_t bytesPerRow, uint32_t rowsPerImage)
{
	wgpu::ImageCopyBuffer imageCopyBuffer{};
	imageCopyBuffer.buffer = buffer;
	imageCopyBuffer.layout = CreateTextureDataLayout(offset, bytesPerRow, rowsPerImage);

	return imageCopyBuffer;
}

wgpu::ImageCopyTexture CreateImageCopyTexture(wgpu::Texture texture, uint32_t mipLevel, wgpu::Origin3D origin, wgpu::TextureAspect aspect) 
{
	wgpu::ImageCopyTexture imageCopyTexture{};
	imageCopyTexture.texture = texture;
	imageCopyTexture.mipLevel = mipLevel;
	imageCopyTexture.origin = origin;
	imageCopyTexture.aspect = aspect;

	return imageCopyTexture;
}

wgpu::Texture CreateTexture(wgpu::Device& device, wgpu::Extent3D size, wgpu::TextureFormat format, const void* data)
{
	wgpu::TextureDescriptor textureDescriptor{};
	textureDescriptor.usage = wgpu::TextureUsage::CopyDst | wgpu::TextureUsage::TextureBinding;
	textureDescriptor.size = size;
	textureDescriptor.format = format;
	wgpu::Texture texture = device.CreateTexture(&textureDescriptor);
	if (data) 
	{
		auto texelBlockSize = dawn::utils::GetTexelBlockSizeInBytes(format);
		wgpu::ImageCopyTexture destination{ .texture = texture };
		wgpu::TextureDataLayout dataLayout{ 
			.bytesPerRow = size.width * texelBlockSize,
			.rowsPerImage = size.height,
		};
		device.GetQueue().WriteTexture(&destination, data, size.width * size.height * texelBlockSize, &dataLayout, &size);
	}
	return texture;
}

wgpu::Texture CreateRenderTexture(wgpu::Device& device, wgpu::Extent3D size, wgpu::TextureFormat format)
{
	wgpu::TextureDescriptor textureDesc{
		.usage = wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::TextureBinding,
		.size = size,
		.format = format,
	};
	return device.CreateTexture(&textureDesc);
}


wgpu::Texture LoadTexture(wgpu::Device& device, std::filesystem::path path)
{
	int width, height, channels;
	unsigned char* pixelData = stbi_load(path.string().c_str(), &width, &height, &channels, 4);

	wgpu::Extent3D size{ static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1 };

	wgpu::TextureDescriptor textureDesc{
		.usage = wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopyDst,
		.dimension = wgpu::TextureDimension::e2D,
		.size = size,
		.format = wgpu::TextureFormat::RGBA8Unorm,
	};
	wgpu::Texture texture = device.CreateTexture(&textureDesc);

	wgpu::ImageCopyTexture destination{ .texture = texture };
	wgpu::TextureDataLayout source{
		.bytesPerRow = 4 * size.width,
		.rowsPerImage = size.height,
	};
	device.GetQueue().WriteTexture( &destination, pixelData, width * height * 4, &source, &size );

	stbi_image_free(pixelData);

	return texture;
}

wgpu::Texture LoadTextureMipmap(wgpu::Device& device, std::filesystem::path path)
{
	int width, height, channels;
	uint8_t* pixelData = stbi_load(path.string().c_str(), &width, &height, &channels, 4);

	wgpu::Extent3D size{ static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1 };
	wgpu::TextureDescriptor textureDesc{
		.usage = wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopyDst,
		.dimension = wgpu::TextureDimension::e2D,
		.size = size,
		.format = wgpu::TextureFormat::RGBA8Unorm,
		.mipLevelCount = (unsigned)std::bit_width(std::max(size.width, size.height)),
	};
	wgpu::Texture texture = device.CreateTexture(&textureDesc);

	// Create image data
	wgpu::Extent3D currSize = size;
	wgpu::Extent3D resizedSize;
	uint8_t* currData = pixelData;
	uint8_t* resizedData = nullptr;
	for (uint32_t level = 0; level < textureDesc.mipLevelCount; ++level) 
	{
		if (level != 0)
		{
			resizedSize = 
			{
				.width = std::max(1u, currSize.width / 2),
				.height = std::max(1u, currSize.height / 2),
			};
			resizedData = new uint8_t[resizedSize.width * resizedSize.height * 4];

			stbir_resize_uint8_linear(
				currData, currSize.width, currSize.height, 0,
				resizedData, resizedSize.width, resizedSize.height, 0, stbir_pixel_layout::STBIR_RGBA
			);

			currSize = resizedSize;
			stbi_image_free(currData);
			currData = resizedData;
		}

		wgpu::ImageCopyTexture destination{
			.texture = texture,
			.mipLevel = level,
		};
		wgpu::TextureDataLayout source{
			.bytesPerRow = 4 * currSize.width,
			.rowsPerImage = currSize.height,
		};
		device.GetQueue().WriteTexture(&destination, currData, currSize.width * currSize.height * 4, &source, &currSize );
	}

	stbi_image_free(currData);

	return texture;
}