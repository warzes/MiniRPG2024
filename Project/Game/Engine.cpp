#include "Engine.h"

#if defined(_MSC_VER)
#	pragma warning(push, 3)
#endif

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/hash.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/norm.hpp>
//#include <glm/gtx/quaternion.hpp>
//#include <glm/gtx/transform.hpp>
//#include <glm/gtx/matrix_decompose.hpp>
//#include <glm/gtx/normal.hpp>

#include <stb/stb_image.h>
#include <glfw.h>

#if defined(_MSC_VER)
#	pragma warning(pop)
#endif

constexpr uint32_t kWidth = 1024;
constexpr uint32_t kHeight = 768;

struct EngineData
{
	GLFWwindow* window = nullptr;
};

void glfwErrorCallback(int, const char* message)
{
	puts(message);
}

void Engine::run(std::unique_ptr<IApp> app)
{
	m_app = std::move(app);
	m_data = new EngineData;

	if (init())
	{
		while (glfwWindowShouldClose(m_data->window) == GLFW_FALSE)
		{
			int display_w, display_h;
			glfwGetFramebufferSize(m_data->window, &display_w, &display_h);

			m_render.Update();
			m_render.Frame();
			glfwPollEvents();
		}
	}

	m_render.Destroy();
	glfwDestroyWindow(m_data->window);
	glfwTerminate();
	delete m_data;
}

bool Engine::init()
{
	glfwSetErrorCallback(glfwErrorCallback);
	if (!glfwInit())
	{
		puts("Could not initialize GLFW!");
		return false;
	}
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_COCOA_RETINA_FRAMEBUFFER, GLFW_FALSE);
	m_data->window = glfwCreateWindow(kWidth, kHeight, "Game", nullptr, nullptr);
	if (!m_data->window)
	{
		puts("Could not open window!");
		return false;
	}

	if (!m_render.Create((void*)m_data->window))
	{
		puts("Render system not create!");
		return false;
	}

	return true;
}