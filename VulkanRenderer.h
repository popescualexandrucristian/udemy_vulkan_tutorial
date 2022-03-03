#pragma once

#include <stdexcept>
#include <vector>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm.hpp>

#include "mesh.h"

const size_t MAX_NUMBER_OF_PROCCESSED_FRAMES_INFLIGHT = 2;

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

struct SwapchainImage
{
   VkImage image;
   VkImageView imageView;
};

class VulkanRenderer
{
public:
   VulkanRenderer();

   int init(GLFWwindow* window);
   void cleanup();

   void draw();

   ~VulkanRenderer();

private:
   void createInstance();
   void hookDebugMessager();
   void creteSurface();
   void checkInstanceExtensionSupported(const char* const* extensionNames, size_t extensionCount) const;
   void getPhysicalDevice();
   void createLogicalDevice();
   bool checkDeviceSwapChainSupport(VkPhysicalDevice device) const;
   uint32_t checkDeviceSutable(VkPhysicalDevice device) const;
   QueueFamilyIndices getQueueFamilyIndices(VkPhysicalDevice device) const;
   SwapchainDetails getSwapchainDetails(VkPhysicalDevice device, VkSurfaceKHR surface) const;
   VkSurfaceFormatKHR selectBestSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats) const;
   VkPresentModeKHR selectBestPresentationMode(const std::vector<VkPresentModeKHR>& presentationModes) const;
   VkExtent2D selectBestResolution(GLFWwindow* window, VkSurfaceCapabilitiesKHR surfaceCapabilityes) const;
   void createSwapChain();
   VkImageView createImageView(VkDevice device, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags) const;
   VkShaderModule createShaderModule(VkDevice device, const std::vector<char>& code) const;
   void createRenderPass();
   void createGraphicsPipeline();
   void createFrameBuffers();
   void createCommandPool();
   void allocateCommandBuffers();
   void recordCommandBuffers();
   void createSyncronization();

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
   VkSwapchainKHR swapChain = VK_NULL_HANDLE;
   std::vector<SwapchainImage> swapChainImages;
   VkExtent2D currentResolution = {};
   VkSurfaceFormatKHR currentSurfaceFormat = { VK_FORMAT_UNDEFINED, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
   VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
   VkRenderPass renderPass = VK_NULL_HANDLE;
   VkPipeline graphicsPipeline = VK_NULL_HANDLE;
   std::vector<VkFramebuffer> swapChainFramebuffers;
   VkCommandPool graphicsCommandPool = VK_NULL_HANDLE;
   std::vector<VkCommandBuffer> commandBuffers;
   std::vector<VkSemaphore> imagesAvailable;
   std::vector<VkFence> drawFences;
   std::vector<VkSemaphore> rendersFinished;
   size_t currentFrame = 0;

   Mesh firstMesh;
};