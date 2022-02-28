#include <inttypes.h>
#include <stdio.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm.hpp>

int main()
{
   glfwInit();

   glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

   GLFWwindow* window = glfwCreateWindow(800, 600, "Test window", nullptr, nullptr);

   uint32_t extensionCount = 0;
   vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
   printf("Extensions count : %i\n", extensionCount);

   glm::mat4x4 testMatrix(1.0f);

   glm::vec4 testVector(1.0f);

   glm::vec4 result = testMatrix * testVector;

   while (!glfwWindowShouldClose(window))
   {
      glfwPollEvents();
   }

   glfwDestroyWindow(window);
   glfwTerminate();
   return 0;
}