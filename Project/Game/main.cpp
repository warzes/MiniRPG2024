#include "Engine.h"
#include "TestApp.h"
//-----------------------------------------------------------------------------
int main(
	[[maybe_unused]] int   argc,
	[[maybe_unused]] char* argv[])
{
	Engine::Run<TestApp>();
	return 0;
}
//-----------------------------------------------------------------------------