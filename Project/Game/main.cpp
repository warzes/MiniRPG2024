#include "Engine.h"
#include <glfw.h>
//-----------------------------------------------------------------------------
void glfwErrorCallback(int, const char* message)
{
	puts(message);
}
//-----------------------------------------------------------------------------
int main(
	[[maybe_unused]] int   argc,
	[[maybe_unused]] char* argv[])
{
	glfwInit();
	glfwSetErrorCallback(glfwErrorCallback);

	GLFWwindow* window = glfwCreateWindow(1024, 768, "Game", nullptr, nullptr);

	while (glfwWindowShouldClose(window) == GLFW_FALSE)
	{
		int display_w, display_h;
		glfwGetFramebufferSize(window, &display_w, &display_h);

		glfwSwapBuffers(window);
		glfwPollEvents();
	}


	glfwTerminate();
	return 0;
}
//-----------------------------------------------------------------------------