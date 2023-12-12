#include "Engine.h"
//-----------------------------------------------------------------------------
class TestApp final : public IApp
{
public:
	TestApp(Engine& engine) : IApp(engine) {}
	AppCreateInfo GetCreateInfo() final { return {}; }

	bool Init() final { return true; }
	virtual void Close() final {}
	virtual void Update() final {}
	virtual void Frame() final {}
};
//-----------------------------------------------------------------------------
int main(
	[[maybe_unused]] int   argc,
	[[maybe_unused]] char* argv[])
{
	Engine::Run<TestApp>();
	return 0;
}
//-----------------------------------------------------------------------------