#pragma once

#include "Engine.h"

class TestApp final : public IApp
{
public:
	TestApp(Engine& engine) : IApp(engine) {}
	AppCreateInfo GetCreateInfo() final { return {}; }

	bool Init() final;
	virtual void Close() final;
	virtual void Update() final;
	virtual void Frame() final;
};