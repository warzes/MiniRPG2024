#include "Engine.h"

static GLFWwindow* window = NULL;
static int swapAndPollInput();

#if defined(_MSC_VER)
#	pragma comment( lib, "OpenGL32.lib" )
#endif

static void run() 
{
	glClearColor(0.1, 0.4, 1.0, 0.0);
	do 
	{
		int display_w, display_h;
		glfwGetFramebufferSize(window, &display_w, &display_h);
		glViewport(0, 0, display_w, display_h);
		glClear(GL_COLOR_BUFFER_BIT);
	} while (swapAndPollInput());
}

static int swapAndPollInput()
{
	glfwSwapBuffers(window);
	glfwPollEvents();
	return glfwWindowShouldClose(window) == GLFW_FALSE;
}

static void glfwErrorCallback(int, const char* message)
{
	//fprintf(stderr, "%s\n", message);
}

static void glMessageCallback(GLenum, GLenum, GLuint, GLenum, GLsizei, const char* message, const void*) 
{
	glfwErrorCallback(0, message);
}
//-----------------------------------------------------------------------------
int main(
	[[maybe_unused]] int   argc,
	[[maybe_unused]] char* argv[])
{
	// GLFW + OpenGL init
	glfwInit();
	glfwSetErrorCallback(glfwErrorCallback);
	glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
	glfwWindowHint(GLFW_CONTEXT_ROBUSTNESS, GLFW_LOSE_CONTEXT_ON_RESET);
	window = glfwCreateWindow(1024, 768, "GL test app", NULL, NULL);
	glfwMakeContextCurrent(window);
	//glDebugMessageCallback(glMessageCallback, NULL);
	//glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL, GL_FALSE);
	//glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_HIGH, 0, NULL, GL_TRUE);
	//glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_MEDIUM, 0, NULL, GL_TRUE);

	// Event loop
	run();
	// Cleanup
	glfwTerminate();
	return 0;
}