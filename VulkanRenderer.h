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
   int32_t presentationFamily = -1;

   bool valid();
};

struct SwapchainDetails
{
   VkSurfaceCapabilitiesKHR capabilities = {};
   std::vector<VkSurfaceFormatKHR> supportedFormats;
   std::vector<VkPresentModeKHR> supportedPresentationModes;
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
   void creteSurface();
   bool checkInstanceExtensionSupported(const char* const* extensionNames, size_t extensionCount) const;
   void getPhysicalDevice();
   void createLogicalDevice();
   bool checkDeviceSwapChainSupport(VkPhysicalDevice device) const;
   uint32_t checkDeviceSutable(VkPhysicalDevice device) const;
   QueueFamilyIndices getQueueFamilyIndices(VkPhysicalDevice device) const;
   SwapchainDetails getSwapchainDetails(VkPhysicalDevice device, VkSurfaceKHR surface) const;

   GLFWwindow* window = nullptr;
   VkInstance instance = VK_NULL_HANDLE;
   struct {
      VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
      VkDevice logicalDevice = VK_NULL_HANDLE;
   } mainDevice;
   QueueFamilyIndices queueFamilyIndices;
   SwapchainDetails swapchainDetails;
   VkQueue graphicsQueue = VK_NULL_HANDLE;
   VkQueue presentationQueue = VK_NULL_HANDLE;
   VkSurfaceKHR surface = VK_NULL_HANDLE;
   VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
};