#pragma once

class Buffer
{
public:
	Buffer();
	virtual ~Buffer();
protected:
	struct BufferData;
	BufferData* m_data = nullptr;
};

class VertexBuffer final : public Buffer
{

};

class IndexBuffer final : public Buffer
{

};

class UniformBuffer final : public Buffer
{

};
