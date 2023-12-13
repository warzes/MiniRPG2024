#include "Engine.h"

#if defined(_MSC_VER)
#	pragma warning(push, 3)
#endif

//#include <glm/gtc/matrix_transform.hpp>
//#include <glm/gtc/type_ptr.hpp>
//#include <glm/gtx/hash.hpp>
//#include <glm/gtx/rotate_vector.hpp>
//#include <glm/gtx/euler_angles.hpp>
//#include <glm/gtx/norm.hpp>
////#include <glm/gtx/quaternion.hpp>
////#include <glm/gtx/transform.hpp>
////#include <glm/gtx/matrix_decompose.hpp>
////#include <glm/gtx/normal.hpp>

//#include <stb/stb_image.h>
#include <glfw.h>

#if defined(_MSC_VER)
#	pragma warning(pop)
#endif

//-----------------------------------------------------------------------------
// global var engine
bool IsEngineClose = false;
int WindowWidth = 0;
int WindowHeight = 0;
constexpr uint32_t kWidth = 1024;
constexpr uint32_t kHeight = 768;
//-----------------------------------------------------------------------------
struct EngineData
{
	GLFWwindow* window = nullptr;
};
//-----------------------------------------------------------------------------
int IApp::GetWindowWidth() const
{
	return WindowWidth;
}
//-----------------------------------------------------------------------------
int IApp::GetWindowHeight() const
{
	return WindowHeight;
}
//-----------------------------------------------------------------------------
void IApp::Exit()
{
	m_engine.exit();
}
//-----------------------------------------------------------------------------
void glfwErrorCallback(int, const char* message) noexcept
{
	Fatal(message);
}
//-----------------------------------------------------------------------------
void Engine::run(std::unique_ptr<IApp> app)
{
	m_app = std::move(app);

	if (init())
	{
		if (m_app->Init())
		{
			while (isRun())
			{
				m_app->Frame();
				m_render.Frame();

				glfwPollEvents();
				glfwGetFramebufferSize(m_data->window, &WindowWidth, &WindowHeight);
				m_app->Update();
			}
		}
		m_app->Close();
	}
	close();
}
//-----------------------------------------------------------------------------
bool Engine::init()
{
	m_data = new EngineData;

	glfwSetErrorCallback(glfwErrorCallback);
	if (!glfwInit())
	{
		Error("Could not initialize GLFW!");
		return false;
	}
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	//glfwWindowHint(GLFW_COCOA_RETINA_FRAMEBUFFER, GLFW_FALSE); // disable high-dpi for macOS
	m_data->window = glfwCreateWindow(kWidth, kHeight, "Game", nullptr, nullptr);
	if (!m_data->window)
	{
		Error("Could not open window!");
		return false;
	}

	if (!m_render.Create((void*)m_data->window))
	{
		Error("Render system not create!");
		return false;
	}

	return true;
}
//-----------------------------------------------------------------------------
void Engine::close()
{
	m_render.Destroy();
	glfwDestroyWindow(m_data->window);
	glfwTerminate();
	delete m_data;
}
//-----------------------------------------------------------------------------
bool Engine::isRun() const
{
	return glfwWindowShouldClose(m_data->window) == GLFW_FALSE && IsEngineClose == false;
}
//-----------------------------------------------------------------------------
void Engine::exit()
{
	IsEngineClose = true;
}
//-----------------------------------------------------------------------------