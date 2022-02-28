#include <inttypes.h>
#include <stdio.h>
#include <vector>

#include "VulkanRenderer.h"

GLFWwindow* createWindow(const std::string& name = "test window", const int width = 800, const int height = 600)
{
   glfwInit();

   glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
   glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

   return glfwCreateWindow(width, height, name.c_str(), nullptr, nullptr);
}

void destroyWindow(GLFWwindow** window)
{
   glfwDestroyWindow(*window);
   *window = nullptr;
   glfwTerminate();
}

#ifdef NDEBUG
int WinMain()
#else
int main()
#endif
{
   GLFWwindow* window = createWindow();

   VulkanRenderer vulkanRenderer;
   if (EXIT_FAILURE == vulkanRenderer.init(window))
      return EXIT_FAILURE;
   

   while (!glfwWindowShouldClose(window))
   {
      glfwPollEvents();
   }

   vulkanRenderer.cleanup();
   destroyWindow(&window);
   return EXIT_SUCCESS;
}