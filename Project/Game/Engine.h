#pragma once

//=============================================================================
// STL and 3rdparty Header
//=============================================================================

#if defined(_MSC_VER)
#	pragma warning(push, 3)
#endif

#include <memory>
#include <string>
#include <vector>
#include <optional>
#include <filesystem>

#if defined(_WIN32)
#endif // _WIN32
#if defined(__EMSCRIPTEN__)
#	include <emscripten/emscripten.h> // Emscripten functionality for C
#	include <emscripten/html5.h>      // Emscripten HTML5 library
#endif // __EMSCRIPTEN__

#include <glmConfig.h>
#include <glm/glm.hpp>

#if defined(_MSC_VER)
#	pragma warning(pop)
#endif

//=============================================================================
// Disable Engine Warning 
//=============================================================================
#if defined(_MSC_VER)
#	pragma warning(push)
#	pragma warning(disable : 4514)
#endif

//=============================================================================
// Engine Header
//=============================================================================
#include "Core.h"
#include "Render.h"

//=============================================================================
// App
//=============================================================================

class Engine;

struct AppCreateInfo
{

};

class IApp
{
public:
	IApp() = delete;
	IApp(IApp&&) = delete;
	IApp(const IApp&) = delete;
	IApp(Engine& engine) : m_engine(engine) {}
	virtual ~IApp() = default;
	IApp& operator=(IApp&&) = delete;
	IApp& operator=(const IApp&) = delete;

	virtual AppCreateInfo GetCreateInfo() = 0;

	virtual bool Init() = 0;
	virtual void Close() = 0;
	virtual void Update() = 0;
	virtual void Frame() = 0;

	float GetDeltaTime() const;

	int GetWindowWidth() const;
	int GetWindowHeight() const;

	void Exit();

protected:
	Engine& m_engine;
};

//=============================================================================
// Engine
//=============================================================================

struct EngineData;
class Engine
{
	friend class IApp;
public:
	template<typename T>
	static void Run()
	{
		Engine engine;
		engine.run(std::make_unique<T>(engine));
	}

private:
	void run(std::unique_ptr<IApp> app);
	bool init();
	void close();
	bool isRun() const;

	void exit();

	std::unique_ptr<IApp> m_app;
	EngineData* m_data = nullptr;
	Render m_render;
};

//=============================================================================
// Default Warning 
//=============================================================================
#if defined(_MSC_VER)
#	pragma warning(pop)
#endif