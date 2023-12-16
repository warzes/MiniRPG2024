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
void onWindowResize(GLFWwindow* window, int /* width */, int /* height */) noexcept
{
	auto that = reinterpret_cast<Engine*>(glfwGetWindowUserPointer(window));
	if (that != nullptr) that->OnResize();
}
//-----------------------------------------------------------------------------
void onMouseMove(GLFWwindow* window, double xpos, double ypos) noexcept
{
	auto that = reinterpret_cast<Engine*>(glfwGetWindowUserPointer(window));
	if (that != nullptr) that->OnMouseMove(xpos, ypos);
}
//-----------------------------------------------------------------------------
void onMouseButton(GLFWwindow* window, int button, int action, int mods) noexcept
{
	auto that = reinterpret_cast<Engine*>(glfwGetWindowUserPointer(window));
	if (that != nullptr) that->OnMouseButton(button, action, mods);
}
//-----------------------------------------------------------------------------
void onScroll(GLFWwindow* window, double xoffset, double yoffset) noexcept
{
	auto that = reinterpret_cast<Engine*>(glfwGetWindowUserPointer(window));
	if (that != nullptr) that->OnScroll(xoffset, yoffset);
}
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

	glfwSetWindowUserPointer(m_data->window, this);
	glfwSetFramebufferSizeCallback(m_data->window, onWindowResize);
	glfwSetCursorPosCallback(m_data->window, onMouseMove);
	glfwSetMouseButtonCallback(m_data->window, onMouseButton);
	glfwSetScrollCallback(m_data->window, onScroll);

	glfwGetFramebufferSize(m_data->window, &WindowWidth, &WindowHeight);

	if (!m_render.Create((void*)m_data->window, WindowWidth, WindowHeight))
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
void Engine::OnResize()
{
	glfwGetFramebufferSize(m_data->window, &WindowWidth, &WindowHeight);
	
	if (!m_render.Resize(WindowWidth, WindowHeight))
		Fatal("render resize error");
}
//-----------------------------------------------------------------------------
void Engine::OnMouseMove(double xpos, double ypos)
{
	//m_render.OnMouseMove(xpos, ypos);
}
//-----------------------------------------------------------------------------
void Engine::OnMouseButton(int button, int action, int mods)
{
	double xpos, ypos;
	glfwGetCursorPos(m_data->window, &xpos, &ypos);
	//m_render.OnMouseButton(button, action, mods, xpos, ypos);
}
//-----------------------------------------------------------------------------
void Engine::OnScroll(double xoffset, double yoffset)
{
	//m_render.OnScroll(xoffset, yoffset);
}
//-----------------------------------------------------------------------------