#include "Engine.h"
//-----------------------------------------------------------------------------
Buffer::Buffer()
{
}
//-----------------------------------------------------------------------------
Buffer::~Buffer()
{
	buffer.Destroy();
	buffer = nullptr;
}
//-----------------------------------------------------------------------------
void VertexBufferLayout::AddAttrib(wgpu::VertexFormat format, uint64_t offset)
{
	wgpu::VertexAttribute attrib;
	attrib.shaderLocation = m_attribs.size();
	attrib.format = format;
	attrib.offset = offset;
	m_attribs.push_back(attrib);
}
//-----------------------------------------------------------------------------
const wgpu::VertexBufferLayout& VertexBufferLayout::Get()
{
	wgpu::VertexBufferLayout vertexBufferLayout{};
	vertexBufferLayout.attributeCount = static_cast<uint32_t>(m_attribs.size());
	vertexBufferLayout.attributes = m_attribs.data();
	vertexBufferLayout.arrayStride = sizeof(m_attribs);
	vertexBufferLayout.stepMode = wgpu::VertexStepMode::Vertex;
	return vertexBufferLayout;
}
//-----------------------------------------------------------------------------
bool VertexBufferLayout::IsZero() const
{
	return m_attribs.size() == 0;
}
//-----------------------------------------------------------------------------
void RenderPipeline::SetPrimitiveState(wgpu::PrimitiveTopology topology, wgpu::IndexFormat stripIndexFormat, wgpu::FrontFace frontFace, wgpu::CullMode cullMode)
{
	m_pipelineDescriptor.primitive.topology = topology;
	m_pipelineDescriptor.primitive.stripIndexFormat = stripIndexFormat;
	m_pipelineDescriptor.primitive.frontFace = frontFace;
	m_pipelineDescriptor.primitive.cullMode = cullMode;
}
//-----------------------------------------------------------------------------
void RenderPipeline::SetBlendState(wgpu::TextureFormat swapChainFormat, wgpu::BlendFactor colorSrcFactor, wgpu::BlendFactor colorDstFactor, wgpu::BlendOperation colorOperation, wgpu::BlendFactor alphaSrcFactor, wgpu::BlendFactor alphaDstFactor, wgpu::BlendOperation alphaOperation, wgpu::ColorWriteMask writeMast)
{
	m_blendState.color.srcFactor = colorSrcFactor;
	m_blendState.color.dstFactor = colorDstFactor;
	m_blendState.color.operation = colorOperation;
	m_blendState.alpha.srcFactor = alphaSrcFactor;
	m_blendState.alpha.dstFactor = alphaDstFactor;
	m_blendState.alpha.operation = alphaOperation;

	m_colorTargetState.format = swapChainFormat;
	m_colorTargetState.blend = &m_blendState;
	m_colorTargetState.writeMask = writeMast;
}
//-----------------------------------------------------------------------------
void RenderPipeline::SetVertexBufferLayout(VertexBufferLayout vertexBufferLayout)
{
	m_vbLayout = vertexBufferLayout;
	if (m_vbLayout.IsZero())
	{
		m_pipelineDescriptor.vertex.bufferCount = 0;
		m_pipelineDescriptor.vertex.buffers = nullptr;
	}
	else
	{
		m_pipelineDescriptor.vertex.bufferCount = 1;
		m_pipelineDescriptor.vertex.buffers = &m_vbLayout.Get();
	}
}
//-----------------------------------------------------------------------------
void RenderPipeline::SetVertexShaderCode(wgpu::ShaderModule shaderModule, const char* entryPoint)
{
	m_pipelineDescriptor.vertex.module = shaderModule;
	m_pipelineDescriptor.vertex.entryPoint = entryPoint;
}
//-----------------------------------------------------------------------------
void RenderPipeline::SetFragmentShaderCode(wgpu::ShaderModule shaderModule, const char* entryPoint)
{
	m_fragmentState.module = shaderModule;
	m_fragmentState.entryPoint = entryPoint;
}
//-----------------------------------------------------------------------------
bool RenderPipeline::Create(wgpu::Device device)
{
	// TODO: в будущем сделать возможность нескольких таргетов
	m_fragmentState.targetCount = 1;
	m_fragmentState.targets = &m_colorTargetState;

	m_pipelineDescriptor.fragment = &m_fragmentState;

	// TODO: multisample
	m_pipelineDescriptor.multisample.count = 1;
	m_pipelineDescriptor.multisample.mask = ~0u;
	m_pipelineDescriptor.multisample.alphaToCoverageEnabled = false;

	pipeline = device.CreateRenderPipeline(&m_pipelineDescriptor);

	return true;
}
//-----------------------------------------------------------------------------
RenderPass::RenderPass()
{
	renderPassColorAttachment.resolveTarget = nullptr;
	renderPassColorAttachment.loadOp = wgpu::LoadOp::Clear;
	renderPassColorAttachment.storeOp = wgpu::StoreOp::Store;
	renderPassColorAttachment.clearValue = wgpu::Color{ 0.05f, 0.05f, 0.05f, 1.0f };

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
}
//-----------------------------------------------------------------------------
void RenderPass::SetTextureView(wgpu::TextureView backBufferView, wgpu::TextureView depthTextureView)
{
	renderPassColorAttachment.view = backBufferView;
	depthStencilAttachment.view = depthTextureView;
}
//-----------------------------------------------------------------------------
void RenderPass::Start(wgpu::CommandEncoder& encoder)
{
	wgpu::RenderPassDescriptor renderPassDescriptor{};
	renderPassDescriptor.timestampWrites = nullptr;

	renderPassDescriptor.colorAttachmentCount = 1;
	renderPassDescriptor.colorAttachments = &renderPassColorAttachment;

	if (depthStencilAttachment.view != nullptr)
		renderPassDescriptor.depthStencilAttachment = &depthStencilAttachment;

	renderPass = encoder.BeginRenderPass(&renderPassDescriptor);
}
//-----------------------------------------------------------------------------
void RenderPass::SetViewport(float x, float y, float width, float height, float minDepth, float maxDepth) const
{
	renderPass.SetViewport(x, y, width, height, minDepth, maxDepth);
}
//-----------------------------------------------------------------------------
void RenderPass::SetScissorRect(uint32_t x, uint32_t y, uint32_t width, uint32_t height) const
{
	renderPass.SetScissorRect(x, y, width, height);
}
//-----------------------------------------------------------------------------
void RenderPass::SetVertexBuffer(uint32_t slot, const VertexBuffer& buffer, uint64_t offset, uint64_t size) const
{
	renderPass.SetVertexBuffer(slot, buffer.buffer, offset, size);
}
//-----------------------------------------------------------------------------
void RenderPass::SetIndexBuffer(const IndexBuffer& buffer, wgpu::IndexFormat format, uint64_t offset, uint64_t size) const
{
	renderPass.SetIndexBuffer(buffer.buffer, format, offset, size);
}
//-----------------------------------------------------------------------------
void RenderPass::SetBindGroup(uint32_t groupIndex, const BindGroup& group, size_t dynamicOffsetCount, const uint32_t* dynamicOffsets) const
{
	renderPass.SetBindGroup(groupIndex, group.bindGroup, dynamicOffsetCount, dynamicOffsets);
}
//-----------------------------------------------------------------------------
void RenderPass::SetPipeline(const RenderPipeline& pipeline) const
{
	renderPass.SetPipeline(pipeline.pipeline);
}
//-----------------------------------------------------------------------------
void RenderPass::SetBlendConstant(const wgpu::Color* color) const
{
	renderPass.SetBlendConstant(color);
}
//-----------------------------------------------------------------------------
void RenderPass::SetStencilReference(uint32_t reference) const
{
	renderPass.SetStencilReference(reference);
}
//-----------------------------------------------------------------------------
void RenderPass::SetLabel(const char* label) const
{
	renderPass.SetLabel(label);
}
//-----------------------------------------------------------------------------
void RenderPass::End()
{
	renderPass.End();
}
//-----------------------------------------------------------------------------
void RenderPass::Draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) const
{
	renderPass.Draw(vertexCount, instanceCount, firstVertex, firstInstance);
}
//-----------------------------------------------------------------------------
void RenderPass::DrawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t baseVertex, uint32_t firstInstance) const
{
	renderPass.DrawIndexed(indexCount, instanceCount, firstIndex, baseVertex, firstInstance);
}
//-----------------------------------------------------------------------------
void RenderPass::DrawIndexedIndirect(const Buffer& indirectBuffer, uint64_t indirectOffset) const
{
	renderPass.DrawIndexedIndirect(indirectBuffer.buffer, indirectOffset);
}
//-----------------------------------------------------------------------------
void RenderPass::DrawIndirect(const Buffer& indirectBuffer, uint64_t indirectOffset) const
{
	renderPass.DrawIndirect(indirectBuffer.buffer, indirectOffset);
}
//-----------------------------------------------------------------------------