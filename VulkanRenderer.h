#pragma once

#include <stdexcept>
#include <vector>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm.hpp>
#include <gtc/matrix_transform.hpp>

#include "mesh.h"

const size_t MAX_NUMBER_OF_PROCCESSED_FRAMES_INFLIGHT = 2;
const size_t MAX_OBJECTS = 2;
const size_t MAX_TEXTURES = 2;

struct QueueFamilyIndices
{
   int32_t graphicFamily = -1; //by the standard this is also a transfer queue, but this can be optimized
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

struct DepthBuffer
{
   VkImage image = VK_NULL_HANDLE;
   VkImageView imageView = VK_NULL_HANDLE;
   VkDeviceMemory deviceMemory = VK_NULL_HANDLE;
   VkFormat format = VK_FORMAT_UNDEFINED;
};

struct DeviceScore
{
   uint32_t deviceScore = 0;
   VkDeviceSize minStorageBufferOffsetAlignment = 0;
};

struct LoadedImage
{
   std::string fileName;
   VkImage image = VK_NULL_HANDLE;
   VkDeviceMemory memory = VK_NULL_HANDLE;
   VkImageView imageView = VK_NULL_HANDLE;
   VkDescriptorSet samplerSet = VK_NULL_HANDLE;
};

class VulkanRenderer
{
public:
   VulkanRenderer();

   int init(GLFWwindow* window);
   void cleanup();

   void draw();

   void updateModelData(size_t index, const UboModel&, const PushModel&);

   ~VulkanRenderer();

private:
   void createInstance();
   void hookDebugMessager();
   void creteSurface();
   void checkInstanceExtensionSupported(const char* const* extensionNames, size_t extensionCount) const;
   void getPhysicalDevice();
   void createLogicalDevice();
   bool checkDeviceSwapChainSupport(VkPhysicalDevice device) const;
   DeviceScore checkDeviceSutable(VkPhysicalDevice device) const;
   QueueFamilyIndices getQueueFamilyIndices(VkPhysicalDevice device) const;
   SwapchainDetails getSwapchainDetails(VkPhysicalDevice device, VkSurfaceKHR surface) const;
   VkSurfaceFormatKHR selectBestSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats) const;
   VkPresentModeKHR selectBestPresentationMode(const std::vector<VkPresentModeKHR>& presentationModes) const;
   VkExtent2D selectBestResolution(GLFWwindow* window, VkSurfaceCapabilitiesKHR surfaceCapabilityes) const;
   void createSwapChain();
   VkFormat choseOptimalImageFormat(const std::vector<VkFormat> formats, VkImageTiling tiling, VkFormatFeatureFlags flags) const;
   void createDepthBuffer();
   VkImage createImage(uint32_t width, uint32_t height, VkFormat format, 
      VkImageTiling tiling, VkImageUsageFlags usageFlags, VkMemoryPropertyFlags propertyFlags, VkDeviceMemory* imageMemory ) const;
   VkImageView createImageView(VkDevice device, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags) const;
   VkShaderModule createShaderModule(VkDevice device, const std::vector<char>& code) const;
   void createRenderPass();
   void createGraphicsPipeline();
   void createFrameBuffers();
   void createCommandPool();
   void allocateCommandBuffers();
   void recordCommandBuffers(size_t frame);
   void createSyncronization();
   void createDescriptorSetLayout();
   void createSamplerDescriptorSetLayout();
   void createDescriptorSet(); //and pool
   void createUniformBuffers();
   void updateUniformBuffers(size_t frame);
   void allocateDynamicBufferTransferSpace();
   uint32_t createTexture(const char* imageFileName);
   void createTextureSampler();
   void createSamplerDescriptorPool();

   GLFWwindow* window = nullptr;
   VkInstance instance = VK_NULL_HANDLE;
   struct {
      VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
      VkDevice logicalDevice = VK_NULL_HANDLE;
      VkDeviceSize minStorageBufferOffsetAlignment = 0;
   } mainDevice;
   QueueFamilyIndices queueFamilyIndices;
   SwapchainDetails swapchainDetails;
   VkQueue graphicsQueue = VK_NULL_HANDLE;
   VkQueue presentationQueue = VK_NULL_HANDLE;
   VkSurfaceKHR surface = VK_NULL_HANDLE;
   VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
   VkSwapchainKHR swapChain = VK_NULL_HANDLE;
   std::vector<SwapchainImage> swapChainImages;
   DepthBuffer depthBuffer;
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

   std::vector<Mesh> meshes;
   size_t modelUniformAlignment = 0;
   UboModel* modelTransferSpace = nullptr;
   std::vector<VkBuffer> dynamicUboBuffers; //one per image buffer
   std::vector<VkDeviceMemory> dynamicUboBuffersMemory;

   struct UboViewProjection
   {
      glm::mat4 projection;
      glm::mat4 view;
   } uboViewProjection;
   std::vector<VkBuffer> uboBuffers;
   std::vector<VkDeviceMemory> uboBuffersMemory; //one per image buffer

   VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
   VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
   std::vector<VkDescriptorSet> descriptorSets;

   std::vector<LoadedImage> loadedTextures;
   VkSampler textureSampler = VK_NULL_HANDLE;
   VkDescriptorPool samplerDescriptorPool;
   VkDescriptorSetLayout samplerDescriptorSetLayout = VK_NULL_HANDLE;
};