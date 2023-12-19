#pragma once

#include <Dawn/webgpu.h>
#include <dawn/webgpu_cpp.h>
#include <Dawn/native/DawnNative.h>
#include <webgpu/webgpu_glfw.h>
#include <Dawn/utils/WGPUHelpers.h>
#include <Dawn/dawn_proc.h>

class Buffer
{
public:
	Buffer();
	virtual ~Buffer();

	wgpu::Buffer buffer = nullptr;

protected:
	bool create(const wgpu::Device& device, wgpu::BufferUsage usage, uint64_t bufferSize, const void* data);
	bool create(const wgpu::Device& device, wgpu::BufferUsage usage, uint64_t count, uint64_t sizeElement, const void* data);
};

class VertexBuffer final : public Buffer
{
public:
	bool Create(const wgpu::Device& device, uint64_t vertexCount, uint64_t vertexSize, const void* data);
};

class IndexBuffer final : public Buffer
{
public:
	bool Create(const wgpu::Device& device, uint64_t indexCount, uint64_t indexSize, const void* data);
};

class UniformBuffer final : public Buffer
{
public:
	bool Create(const wgpu::Device& device, uint64_t size, const void* data);
};

class BindGroup
{
public:
	wgpu::BindGroup bindGroup = nullptr;
};

class BindGroupLayout
{
public:
	void AddVertexUniform();
	void Create();

	wgpu::PipelineLayout CreatePipelineLayout() const;

	wgpu::BindGroupLayout layout = nullptr;
};

class PipelineLayout
{
public:
	wgpu::PipelineLayout layout = nullptr;
};

class VertexBufferLayout
{
public:
	// example offset = offsetof(VertexAttributes, position);
	void SetVertexSize(uint64_t size);
	void AddAttrib(wgpu::VertexFormat format, uint64_t offset);
	void AddAttrib(uint32_t shaderLocation, wgpu::VertexFormat format, uint64_t offset);
	wgpu::VertexBufferLayout Get() const;
	
	bool IsZero() const;

private:
	std::vector<wgpu::VertexAttribute> m_attribs;
	uint64_t m_size = 0;
};

class RenderPipeline
{
public:
	void SetPrimitiveState(
		wgpu::PrimitiveTopology topology = wgpu::PrimitiveTopology::TriangleList, 
		wgpu::IndexFormat stripIndexFormat = wgpu::IndexFormat::Undefined, 
		wgpu::FrontFace frontFace = wgpu::FrontFace::CCW, 
		wgpu::CullMode cullMode = wgpu::CullMode::None);

	void SetBlendState(
		wgpu::TextureFormat swapChainFormat,
		wgpu::BlendFactor colorSrcFactor = wgpu::BlendFactor::SrcAlpha,
		wgpu::BlendFactor colorDstFactor = wgpu::BlendFactor::OneMinusSrcAlpha,
		wgpu::BlendOperation colorOperation = wgpu::BlendOperation::Add,
		wgpu::BlendFactor alphaSrcFactor = wgpu::BlendFactor::Zero,
		wgpu::BlendFactor alphaDstFactor = wgpu::BlendFactor::One,
		wgpu::BlendOperation alphaOperation = wgpu::BlendOperation::Add,
		wgpu::ColorWriteMask writeMast = wgpu::ColorWriteMask::All);

	void SetDepthStencilState(wgpu::DepthStencilState depthStencilState);

	void SetVertexBufferLayout(VertexBufferLayout vertexBufferLayout);
	void SetVertexBufferLayout(const std::vector<VertexBufferLayout>& vertexBufferLayout);
	void SetVertexShaderCode(wgpu::ShaderModule shaderModule, const char* entryPoint = "vs_main");
	void SetFragmentShaderCode(wgpu::ShaderModule shaderModule, const char* entryPoint = "fs_main");
	void SetPipelineLayout(const PipelineLayout& layout); // ===> может BindGroupLayout???

	bool Create(wgpu::Device device);

	wgpu::RenderPipeline pipeline = nullptr;
private:
	wgpu::RenderPipelineDescriptor m_pipelineDescriptor{};
	wgpu::BlendState m_blendState{};
	wgpu::ColorTargetState m_colorTargetState{};
	std::vector<VertexBufferLayout> m_vbLayout;
	std::vector<wgpu::VertexBufferLayout> m_privateLayout;
	wgpu::FragmentState m_fragmentState{};
	wgpu::DepthStencilState m_depthStencilState{};
};

class RenderPass
{
public:
	RenderPass();
	void SetTextureView(wgpu::TextureView backBufferView, wgpu::TextureView depthTextureView = nullptr);

	void Start(wgpu::CommandEncoder& encoder);

	void SetViewport(float x, float y, float width, float height, float minDepth, float maxDepth) const;
	void SetScissorRect(uint32_t x, uint32_t y, uint32_t width, uint32_t height) const;
	void SetVertexBuffer(uint32_t slot, const VertexBuffer& buffer, uint64_t offset = 0, uint64_t size = UINT64_MAX) const;
	void SetIndexBuffer(const IndexBuffer& buffer, wgpu::IndexFormat format, uint64_t offset = 0, uint64_t size = UINT64_MAX) const;

	void SetBindGroup(uint32_t groupIndex, const BindGroup& group, size_t dynamicOffsetCount = 0, const uint32_t* dynamicOffsets = nullptr) const;
	void SetPipeline(const RenderPipeline& pipeline) const;
	void SetBlendConstant(const wgpu::Color* color) const;
	void SetStencilReference(uint32_t reference) const;

	void SetLabel(const char* label) const;

	void Draw(uint32_t vertexCount, uint32_t instanceCount = 1, uint32_t firstVertex = 0, uint32_t firstInstance = 0) const;
	void DrawIndexed(uint32_t indexCount, uint32_t instanceCount = 1, uint32_t firstIndex = 0, int32_t baseVertex = 0, uint32_t firstInstance = 0) const;
	void DrawIndexedIndirect(const Buffer& indirectBuffer, uint64_t indirectOffset) const;
	void DrawIndirect(const Buffer& indirectBuffer, uint64_t indirectOffset) const;

	void End();

	wgpu::RenderPassColorAttachment renderPassColorAttachment{};
	wgpu::RenderPassDepthStencilAttachment depthStencilAttachment{};
	wgpu::RenderPassEncoder renderPass = nullptr;
};