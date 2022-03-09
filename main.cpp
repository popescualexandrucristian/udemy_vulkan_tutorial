#include <inttypes.h>
#include <stdio.h>
#include <vector>

#include "VulkanRenderer.h"
#include "utils.h"
#include <assimp/Importer.hpp>

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

#ifdef WINMAIN
int WinMain()
#else
int main()
#endif
{
   GLFWwindow* window = createWindow();

   {
      VulkanRenderer vulkanRenderer;
      if (EXIT_FAILURE == vulkanRenderer.init(window, true))
         return EXIT_FAILURE;

      double lastTime = 0.0f;
      float angle = 0.0f;


      while (!glfwWindowShouldClose(window))
      {
         double currentTime = glfwGetTime();
         double deltaTime = currentTime - lastTime;
         lastTime = currentTime;

         angle += 20.0f * static_cast<float>(deltaTime);
         if (angle > 360.0f)
            angle = 0.0f;

         glm::mat4 transform = glm::rotate(glm::translate(glm::identity<glm::mat4>(), { 0.0f ,0.0f ,0.0f }), glm::radians(angle), glm::vec3(0.0f, 0.0f, 1.0f));
         vulkanRenderer.updateModelData(0, transform);
         try
         {
            vulkanRenderer.draw();
         }
         catch (std::exception& e)
         {
            printf("Error while drawing : %s\n", e.what());
            break;
         }
         glfwPollEvents();
      }
   }

   destroyWindow(&window);
   return EXIT_SUCCESS;
}