#pragma once

#include <stdexcept>
#include <vector>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm.hpp>

struct QueueFamilyIndices
{
   int32_t graphicFamily = -1;

   bool valid();
};

class VulkanRenderer
{
public:
   VulkanRenderer();

   int init(GLFWwindow* window);
   void cleanup();

   ~VulkanRenderer();

private:
   void createInstance();
   void hookDebugMessager();
   bool checkInstanceExtensionSupported(const char* const* extensionNames, size_t extensionCount) const;
   void getPhysicalDevice();
   void createLogicalDevice();
   uint32_t checkDeviceSutable(VkPhysicalDevice device) const;
   QueueFamilyIndices getQueueFamilyIndices(VkPhysicalDevice device) const;

   GLFWwindow* window = nullptr;
   VkInstance instance = VK_NULL_HANDLE;
   struct {
      VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
      VkDevice logicalDevice = VK_NULL_HANDLE;
   } mainDevice;
   QueueFamilyIndices queueFamilyIndices;
   VkQueue graphicsQueue = VK_NULL_HANDLE;
   VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
};