#include "VulkanRenderer.h"
#include "utils.h"

#include <map>
#include <set>
#include <array>
#include <gtc/matrix_transform.hpp>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

VulkanRenderer::VulkanRenderer()
{
}

int VulkanRenderer::init(GLFWwindow* window, bool useFixedCommandBufferRecordings)
{
   this->window = window;
   this->useFixedCommandBufferRecordings = useFixedCommandBufferRecordings;
   try
   {
      //setup
      createInstance();
      hookDebugMessager();
      creteSurface();
      getPhysicalDevice();
      queueFamilyIndices = getQueueFamilyIndices(mainDevice.physicalDevice);
      swapchainDetails = getSwapchainDetails(mainDevice.physicalDevice, surface);
      createLogicalDevice();// and logical queues
      createSwapChain(); // and swapchain images
      allocateDynamicBufferTransferSpace();
      depthBufferFormat = choseOptimalImageFormat(
         { VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D32_SFLOAT, VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
         VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
      colorBufferFormat = choseOptimalImageFormat(
         { VK_FORMAT_R8G8B8A8_UNORM },
         VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_2_COLOR_ATTACHMENT_BIT);
      createRenderPass();

      //data dependent
      createSubPassADescriptorSetLayout();
      createSubPassBDescriptorSetLayout();
      createSubPassASamplerDescriptorSetLayout();
      createGraphicsPipeline();
      createDepthBuffer();
      createColorBuffer();
      createFrameBuffers();
      createCommandPool();
      createSamplerDescriptorPool();
      createTextureSampler();

      uboViewProjection.projection = glm::perspective(glm::radians(45.0f), static_cast<float>(currentResolution.width) / currentResolution.height, 0.1f, 100.0f);
      uboViewProjection.view = glm::lookAt(glm::vec3(50.0f,50.0f,50.0f), glm::vec3(0.0f, 0.0f, 10.0f), glm::vec3(0.0f,0.0f,1.0f));

      uboViewProjection.projection[1][1] *= -1.0;

      loadTexture("uv-test.png");//default texture

      //render something
      allocateCommandBuffers();
      createSyncronization();
      createUniformBuffers();
      crateSubPassABufferDescriptorSetPool();
      crateSubPassBInputDescriptorSetPool();
      createSubPassABufferDescriptorSet();
      createSubPassBInputDescriptorSet();

      if (useFixedCommandBufferRecordings)
      {
         updateRenderCommands();
      }
   }
   catch (const std::runtime_error& e)
   {
      printf("ERROR : %s\n", e.what());
      return EXIT_FAILURE;
   }

   return EXIT_SUCCESS;
}

void VulkanRenderer::cleanup()
{
   vkDeviceWaitIdle(mainDevice.logicalDevice);

   for (auto& i : loadedTextures)
   {
      vkFreeMemory(mainDevice.logicalDevice, i.memory, nullptr);
      vkDestroyImageView(mainDevice.logicalDevice, i.imageView, nullptr);
      vkDestroyImage(mainDevice.logicalDevice, i.image, nullptr);
      vkFreeDescriptorSets(mainDevice.logicalDevice, subPassASamplerDescriptorPool, 1, &i.samplerSet);
   }
   loadedTextures.clear();

   if (textureSampler != VK_NULL_HANDLE)
      vkDestroySampler(mainDevice.logicalDevice, textureSampler, nullptr);
   textureSampler = VK_NULL_HANDLE;

   if(modelTransferSpace)
      _aligned_free(modelTransferSpace);
   modelTransferSpace = nullptr;

   for(auto& m : meshes)
      m.clean();

   for (auto& i : uboBuffersMemory)
   {
      if (i != VK_NULL_HANDLE)
         vkFreeMemory(mainDevice.logicalDevice, i, nullptr);
   }
   uboBuffersMemory.clear();

   for (auto& i : dynamicUboBuffersMemory)
   {
      if (i != VK_NULL_HANDLE)
         vkFreeMemory(mainDevice.logicalDevice, i, nullptr);
   }
   dynamicUboBuffersMemory.clear();

   if (!subPassBInputDescriptorSets.empty())
      vkFreeDescriptorSets(mainDevice.logicalDevice, subPassBInputsDescriptorPool, static_cast<uint32_t>(subPassBInputDescriptorSets.size()), subPassBInputDescriptorSets.data());
   subPassBInputDescriptorSets.clear();

   if(subPassBInputsDescriptorPool != VK_NULL_HANDLE)
      vkDestroyDescriptorPool(mainDevice.logicalDevice, subPassBInputsDescriptorPool, nullptr);
   subPassBInputsDescriptorPool = VK_NULL_HANDLE;

   if (!subPassABufferDescriptorSets.empty())
      vkFreeDescriptorSets(mainDevice.logicalDevice, subPassABufferDescriptorPool, static_cast<uint32_t>(subPassABufferDescriptorSets.size()), subPassABufferDescriptorSets.data());
   subPassABufferDescriptorSets.clear();

   if (subPassABufferDescriptorPool != VK_NULL_HANDLE)
      vkDestroyDescriptorPool(mainDevice.logicalDevice, subPassABufferDescriptorPool, nullptr);
   subPassABufferDescriptorPool = VK_NULL_HANDLE;

   //the descriptor sets are with the images
   if (subPassASamplerDescriptorPool != VK_NULL_HANDLE)
      vkDestroyDescriptorPool(mainDevice.logicalDevice, subPassASamplerDescriptorPool, nullptr);
   subPassASamplerDescriptorPool = VK_NULL_HANDLE;

   for (auto& i : uboBuffers)
   {
      if (i != VK_NULL_HANDLE)
         vkDestroyBuffer(mainDevice.logicalDevice, i, nullptr);
   }
   uboBuffers.clear();

   for (auto& i : dynamicUboBuffers)
   {
      if (i != VK_NULL_HANDLE)
         vkDestroyBuffer(mainDevice.logicalDevice, i, nullptr);
   }
   dynamicUboBuffers.clear();

   for (size_t i = 0; i < drawFences.size(); ++i)
   {
      if (drawFences[i] != VK_NULL_HANDLE)
         vkDestroyFence(mainDevice.logicalDevice, drawFences[i], nullptr);
   }
   drawFences.clear();

   for (size_t i = 0; i < imagesAvailable.size(); ++i)
   {
      if (imagesAvailable[i] != VK_NULL_HANDLE)
         vkDestroySemaphore(mainDevice.logicalDevice, imagesAvailable[i], nullptr);
   }
   imagesAvailable.clear();

   for (size_t i = 0; i < rendersFinished.size(); ++i)
   {
      if (rendersFinished[i] != VK_NULL_HANDLE)
         vkDestroySemaphore(mainDevice.logicalDevice, rendersFinished[i], nullptr);
   }
   rendersFinished.clear();

   if (graphicsCommandPool != VK_NULL_HANDLE)
      vkDestroyCommandPool(mainDevice.logicalDevice, graphicsCommandPool, nullptr);

   graphicsCommandPool = VK_NULL_HANDLE;

   for (auto& depthBuffer : depthBuffers)
      depthBuffer.clean(mainDevice.logicalDevice);

   for (auto& colorBuffer : colorBuffers)
      colorBuffer.clean(mainDevice.logicalDevice);

   for (auto& framebuffer : swapChainFramebuffers)
   {
      vkDestroyFramebuffer(mainDevice.logicalDevice, framebuffer, nullptr);
   }
   swapChainFramebuffers.clear();

   if (subPassAGraphicsPipeline != VK_NULL_HANDLE)
      vkDestroyPipeline(mainDevice.logicalDevice, subPassAGraphicsPipeline, nullptr);

   subPassAGraphicsPipeline = VK_NULL_HANDLE;

   if (subPassAPipelineLayout != VK_NULL_HANDLE)
      vkDestroyPipelineLayout(mainDevice.logicalDevice, subPassAPipelineLayout, nullptr);

   subPassAPipelineLayout = VK_NULL_HANDLE;

   if (subPassBGraphicsPipeline != VK_NULL_HANDLE)
      vkDestroyPipeline(mainDevice.logicalDevice, subPassBGraphicsPipeline, nullptr);

   subPassBGraphicsPipeline = VK_NULL_HANDLE;

   if (subPassBPipelineLayout != VK_NULL_HANDLE)
      vkDestroyPipelineLayout(mainDevice.logicalDevice, subPassBPipelineLayout, nullptr);

   subPassBPipelineLayout = VK_NULL_HANDLE;

   if (subPassADescriptorSetLayout != VK_NULL_HANDLE)
      vkDestroyDescriptorSetLayout(mainDevice.logicalDevice, subPassADescriptorSetLayout, nullptr);
   subPassADescriptorSetLayout = VK_NULL_HANDLE;

   if (subPassBDescriptorSetLayout != VK_NULL_HANDLE)
      vkDestroyDescriptorSetLayout(mainDevice.logicalDevice, subPassBDescriptorSetLayout, nullptr);
   subPassBDescriptorSetLayout = VK_NULL_HANDLE;

   if (samplerDescriptorSetLayout != VK_NULL_HANDLE)
      vkDestroyDescriptorSetLayout(mainDevice.logicalDevice, samplerDescriptorSetLayout, nullptr);

   samplerDescriptorSetLayout = VK_NULL_HANDLE;

   if (renderPass != VK_NULL_HANDLE)
      vkDestroyRenderPass(mainDevice.logicalDevice, renderPass, nullptr);

   renderPass = VK_NULL_HANDLE;

   for (auto& img : swapChainImages)
   {
      if (img.imageView != VK_NULL_HANDLE)
         vkDestroyImageView(mainDevice.logicalDevice, img.imageView, nullptr);

      img.imageView = VK_NULL_HANDLE;
   }
   swapChainImages.clear();

   if (VK_NULL_HANDLE != swapChain)
      vkDestroySwapchainKHR(mainDevice.logicalDevice, swapChain, nullptr);

   swapChain = VK_NULL_HANDLE;

   if (VK_NULL_HANDLE != mainDevice.logicalDevice)
      vkDestroyDevice(mainDevice.logicalDevice, nullptr);

   mainDevice.logicalDevice = VK_NULL_HANDLE;

   if (VK_NULL_HANDLE != surface)
      vkDestroySurfaceKHR(instance, surface, nullptr);

   surface = VK_NULL_HANDLE;

#ifdef ENABLE_VALIDATION
   auto vkDestroyDebugUtilsMessengerEXTFunc = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
   if (vkDestroyDebugUtilsMessengerEXTFunc && VK_NULL_HANDLE != debugMessenger)
      vkDestroyDebugUtilsMessengerEXTFunc(instance, debugMessenger, nullptr);

   debugMessenger = VK_NULL_HANDLE;
#endif
   if (VK_NULL_HANDLE != instance)
      vkDestroyInstance(instance, nullptr);

   instance = VK_NULL_HANDLE;
}

void VulkanRenderer::draw()
{
   //TODO recreate swapchain and framebuffers if needed here if the results are invalid

   if (VK_SUCCESS != vkWaitForFences(mainDevice.logicalDevice, 1, &drawFences[currentFrame % MAX_NUMBER_OF_PROCCESSED_FRAMES_INFLIGHT], VK_TRUE, UINT64_MAX))
      throw std::runtime_error("Unable to get unused image");

   if (VK_SUCCESS != vkResetFences(mainDevice.logicalDevice, 1, &drawFences[currentFrame % MAX_NUMBER_OF_PROCCESSED_FRAMES_INFLIGHT]))
      throw std::runtime_error("Unable to reset fence for used image");

   uint32_t imageIndex = 0;
   VkResult aquieredImage = vkAcquireNextImageKHR(
      mainDevice.logicalDevice, 
      swapChain, UINT64_MAX, 
      imagesAvailable[currentFrame % MAX_NUMBER_OF_PROCCESSED_FRAMES_INFLIGHT], 
      VK_NULL_HANDLE, 
      &imageIndex);

   updateUniformBuffers(imageIndex);
   if (!useFixedCommandBufferRecordings)
      recordCommandBuffers(imageIndex);

   if (VK_SUCCESS != aquieredImage)
      throw std::runtime_error("Unable to get next swapchain image");

   VkSubmitInfo submitInfo = {};
   submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
   
   VkPipelineStageFlags waitStages = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
   submitInfo.pWaitSemaphores = &imagesAvailable[currentFrame % MAX_NUMBER_OF_PROCCESSED_FRAMES_INFLIGHT];
   submitInfo.pWaitDstStageMask = &waitStages; //wait unit signaled
   submitInfo.waitSemaphoreCount = 1;
   
   submitInfo.pCommandBuffers = &commandBuffers[imageIndex];
   submitInfo.commandBufferCount = 1;

   submitInfo.pSignalSemaphores = &rendersFinished[currentFrame % MAX_NUMBER_OF_PROCCESSED_FRAMES_INFLIGHT]; // signaled when presented
   submitInfo.signalSemaphoreCount = 1;

   VkResult queueSubmited = vkQueueSubmit(graphicsQueue, 1, &submitInfo, drawFences[currentFrame % MAX_NUMBER_OF_PROCCESSED_FRAMES_INFLIGHT]);
   if (VK_SUCCESS != queueSubmited)
      throw std::runtime_error("Unable to submit");

   VkPresentInfoKHR presentInfo = {};
   presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
   presentInfo.pWaitSemaphores = &rendersFinished[currentFrame % MAX_NUMBER_OF_PROCCESSED_FRAMES_INFLIGHT];
   presentInfo.waitSemaphoreCount = 1;
   presentInfo.pSwapchains = &swapChain;
   presentInfo.swapchainCount = 1;
   presentInfo.pImageIndices = &imageIndex;

   ++currentFrame;

   VkResult imagePresented = vkQueuePresentKHR(presentationQueue, &presentInfo);
   if (VK_SUCCESS != imagePresented)
      throw std::runtime_error("Unable to present image");
}

VulkanRenderer::~VulkanRenderer()
{
   cleanup();
}

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
   VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
   VkDebugUtilsMessageTypeFlagsEXT messageType,
   const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
   void* pUserData) {
   printf("validation layer: %s\n", pCallbackData->pMessage);

#ifdef _DEBUG
   if (messageSeverity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
      __debugbreak();
#endif

   return VK_FALSE;
}

void VulkanRenderer::createInstance()
{
   VkApplicationInfo applicationInfo = {};
   applicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;

   applicationInfo.apiVersion = VK_API_VERSION_1_1;

   applicationInfo.pApplicationName = "Vulkan application";
   applicationInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);

   applicationInfo.pEngineName = "No engine";
   applicationInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);

   VkInstanceCreateInfo createInfo = {};
   createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
   createInfo.pApplicationInfo = &applicationInfo;

   uint32_t glfwExtensionsCount = 0;
   std::vector<const char*> extensionNames;
#ifdef ENABLE_VALIDATION
   extensionNames.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif
   const char** extensionNamesGLFW = glfwGetRequiredInstanceExtensions(&glfwExtensionsCount);
   for (size_t i = 0; i < glfwExtensionsCount; ++i)
      extensionNames.push_back(extensionNamesGLFW[i]);
   createInfo.ppEnabledExtensionNames = extensionNames.data();
   createInfo.enabledExtensionCount = static_cast<uint32_t>(extensionNames.size());
   checkInstanceExtensionSupported(createInfo.ppEnabledExtensionNames, createInfo.enabledExtensionCount);

#ifdef ENABLE_VALIDATION
   const char* validationLayers = "VK_LAYER_KHRONOS_validation" ;
   createInfo.ppEnabledLayerNames = &validationLayers;
   createInfo.enabledLayerCount = 1;
#endif

   VkResult result = vkCreateInstance(&createInfo, nullptr, &instance);

   if (result != VK_SUCCESS)
      throw std::runtime_error("Failed to create instance");
}

void VulkanRenderer::hookDebugMessager()
{
#ifndef ENABLE_VALIDATION
   return;
#endif
   VkDebugUtilsMessengerCreateInfoEXT createInfo = {};
   createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
   createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
   createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
   createInfo.pfnUserCallback = debugCallback;
   createInfo.pUserData = nullptr;

   auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
   if (!func || VK_SUCCESS != func(instance, &createInfo, nullptr, &debugMessenger))
      throw std::runtime_error("Could not hook debug messenger");
}

void VulkanRenderer::createLogicalDevice()
{
   std::set<int32_t> usedQueues;
   usedQueues.insert(queueFamilyIndices.graphicFamily);
   usedQueues.insert(queueFamilyIndices.presentationFamily);
   std::vector<VkDeviceQueueCreateInfo> queueCreateionInfos;
   for (auto& i : usedQueues)
   {
      VkDeviceQueueCreateInfo queueCreateInfo = {};
      queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
      queueCreateInfo.queueFamilyIndex = i;
      float priority = 1.0f;
      queueCreateInfo.pQueuePriorities = &priority;
      queueCreateInfo.queueCount = 1;

      queueCreateionInfos.push_back(queueCreateInfo);
   }


   VkDeviceCreateInfo createInfo = {};
   createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
   createInfo.pQueueCreateInfos = queueCreateionInfos.data();
   createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateionInfos.size());

   createInfo.enabledExtensionCount = 1;
   const char* extensionNames = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
   createInfo.ppEnabledExtensionNames = &extensionNames;

   VkPhysicalDeviceFeatures deviceFeatures = {};
   createInfo.pEnabledFeatures = &deviceFeatures;
   //TODO : Add device features

   VkDevice device = VK_NULL_HANDLE;
   if (VK_SUCCESS != vkCreateDevice(mainDevice.physicalDevice, &createInfo, nullptr, &device))
      throw std::runtime_error("Could not create logical device");

   mainDevice.logicalDevice = device;

   vkGetDeviceQueue(mainDevice.logicalDevice, queueFamilyIndices.graphicFamily, 0, &graphicsQueue);
   vkGetDeviceQueue(mainDevice.logicalDevice, queueFamilyIndices.presentationFamily, 0, &presentationQueue);
}

QueueFamilyIndices VulkanRenderer::getQueueFamilyIndices(VkPhysicalDevice device) const
{
   uint32_t queueCount = 0;
   vkGetPhysicalDeviceQueueFamilyProperties(device, &queueCount, nullptr);

   std::vector<VkQueueFamilyProperties> properties;
   properties.resize(queueCount);

   vkGetPhysicalDeviceQueueFamilyProperties(device, &queueCount, properties.data());

   QueueFamilyIndices out;
   int32_t index = 0;
   for (auto& property : properties)
   {
      if (property.queueFlags & VK_QUEUE_GRAPHICS_BIT && property.queueCount > 0)
      {
         out.graphicFamily = index;
      }
      VkBool32 supportPresentation = false;
      if (VK_SUCCESS == vkGetPhysicalDeviceSurfaceSupportKHR(device, index, surface, &supportPresentation) && supportPresentation && property.queueCount > 0)
      {
         out.presentationFamily = index;
      }

      ++index;

      if (out.valid())
      {
         break;
      }
   }

   return out;
}

bool VulkanRenderer::checkDeviceSwapChainSupport(VkPhysicalDevice device) const
{
   uint32_t extensionCount = 0;
   if (VK_SUCCESS != vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr))
      throw std::runtime_error("Could not enumerate device extensions");

   std::vector<VkExtensionProperties> extensions;
   extensions.resize(extensionCount);
   if (VK_SUCCESS != vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, extensions.data()))
      throw std::runtime_error("Could not enumerate device extensions");

   for (auto& extension : extensions)
      if (strcmp(extension.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0)
         return true;

   return false;
}

void VulkanRenderer::creteSurface()
{
   if (VK_SUCCESS != glfwCreateWindowSurface(instance, window, nullptr, &surface))
      throw std::runtime_error("Unable to create a surface");
}

SwapchainDetails VulkanRenderer::getSwapchainDetails(VkPhysicalDevice device, VkSurfaceKHR surface) const
{
   SwapchainDetails out = {};

   if (VK_SUCCESS != vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &out.capabilities))
      throw std::runtime_error("Could not get surface capabilities");

   uint32_t surfaceFormatsCount = 0;
   if (VK_SUCCESS != vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &surfaceFormatsCount, nullptr))
      throw std::runtime_error("Could not enumerate formats");

   std::vector<VkSurfaceFormatKHR> formats;
   formats.resize(surfaceFormatsCount);

   if (VK_SUCCESS != vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &surfaceFormatsCount, formats.data()))
      throw std::runtime_error("Could not enumerate formats");


   uint32_t presentationModeCount = 0;
   if (VK_SUCCESS != vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentationModeCount, nullptr))
      throw std::runtime_error("Could not enumerate formats");

   std::vector<VkPresentModeKHR> presentationModes;
   presentationModes.resize(presentationModeCount);

   if (VK_SUCCESS != vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentationModeCount, presentationModes.data()))
      throw std::runtime_error("Could not enumerate formats");

   out.supportedFormats.swap(formats);
   out.supportedPresentationModes.swap(presentationModes);

   return out;
}

void VulkanRenderer::updateModelData(size_t index, const glm::mat4& transform, const PushModel& pushData)
{
   if (meshes.size() <= index)
      return;

   meshes[index].setModel(transform);
   meshes[index].setPushData(pushData);
}

VkSurfaceFormatKHR VulkanRenderer::selectBestSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats) const
{
   if (formats.empty())
      throw std::runtime_error("No formats to select from");

   if (formats.size() == 1 && formats[0].format == VK_FORMAT_UNDEFINED)
      return { VK_FORMAT_R8G8B8A8_UNORM ,VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };

   for (auto& format : formats)
   {
      if ((format.format == VK_FORMAT_B8G8R8A8_UNORM || format.format == VK_FORMAT_R8G8B8A8_UNORM) && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
         return format;
   }

   return formats[0];
}

VkPresentModeKHR VulkanRenderer::selectBestPresentationMode(const std::vector<VkPresentModeKHR>& presentationModes) const
{
   bool hasMailBox = false;
   bool hasFifo = false;
   for (auto mode : presentationModes)
   {
      if (mode == VkPresentModeKHR::VK_PRESENT_MODE_MAILBOX_KHR)
         hasMailBox = true;

      if (mode == VkPresentModeKHR::VK_PRESENT_MODE_FIFO_KHR)
         hasFifo = true;
   }

   if (hasMailBox)
      return VkPresentModeKHR::VK_PRESENT_MODE_MAILBOX_KHR;

   if (hasFifo)
      return VkPresentModeKHR::VK_PRESENT_MODE_FIFO_KHR;

   return presentationModes[0];
}

void VulkanRenderer::createGraphicsPipeline()
{
   VkGraphicsPipelineCreateInfo createInfo = {};
   createInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;

   //shader modules
   std::vector<char> vertexShader = readFile("shader.vert.spv");
   std::vector<char> fragmentShader = readFile("shader.frag.spv");

   VkShaderModule vertexShaderModule = createShaderModule(mainDevice.logicalDevice, vertexShader);
   VkShaderModule fragmentShaderModule = createShaderModule(mainDevice.logicalDevice, fragmentShader);

   VkPipelineShaderStageCreateInfo vertexShaderCreateInfo = {};
   vertexShaderCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
   vertexShaderCreateInfo.module = vertexShaderModule;
   vertexShaderCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
   vertexShaderCreateInfo.pName = "main";

   VkPipelineShaderStageCreateInfo fragmentShaderCreateInfo = {};
   fragmentShaderCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
   fragmentShaderCreateInfo.module = fragmentShaderModule;
   fragmentShaderCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
   fragmentShaderCreateInfo.pName = "main";
   
   VkPipelineShaderStageCreateInfo shaderStages[] = { vertexShaderCreateInfo, fragmentShaderCreateInfo };
   createInfo.pStages = shaderStages;
   createInfo.stageCount = 2;

   //vertex input

   VkVertexInputBindingDescription bindingDescription = {};
   bindingDescription.binding = 0;
   bindingDescription.stride = sizeof(Vertex);
   bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

   std::vector<VkVertexInputAttributeDescription> attributeDescriptions;

   VkVertexInputAttributeDescription positionVertexAttributeDescription = {};
   positionVertexAttributeDescription.binding = 0; //match bindingDescription.binding
   positionVertexAttributeDescription.location = 0; // layout location in shader
   positionVertexAttributeDescription.format = VK_FORMAT_R32G32B32_SFLOAT;
   positionVertexAttributeDescription.offset = offsetof(Vertex, Vertex::position);
   attributeDescriptions.push_back(positionVertexAttributeDescription);

   VkVertexInputAttributeDescription colorVertexAttributeDescription = {};
   colorVertexAttributeDescription.binding = 0;
   colorVertexAttributeDescription.location = 1;
   colorVertexAttributeDescription.format = VK_FORMAT_R32G32B32_SFLOAT;
   colorVertexAttributeDescription.offset = offsetof(Vertex, Vertex::color);
   attributeDescriptions.push_back(colorVertexAttributeDescription);

   VkVertexInputAttributeDescription uvVertexAttributeDescription = {};
   uvVertexAttributeDescription.binding = 0;
   uvVertexAttributeDescription.location = 2;
   uvVertexAttributeDescription.format = VK_FORMAT_R32G32_SFLOAT;
   uvVertexAttributeDescription.offset = offsetof(Vertex, Vertex::uv);
   attributeDescriptions.push_back(uvVertexAttributeDescription);

   VkPipelineVertexInputStateCreateInfo vertexInputCreateInfo = {};
   vertexInputCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
   vertexInputCreateInfo.vertexBindingDescriptionCount = 1;
   vertexInputCreateInfo.pVertexBindingDescriptions = &bindingDescription;
   vertexInputCreateInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
   vertexInputCreateInfo.pVertexAttributeDescriptions = attributeDescriptions.data();
   createInfo.pVertexInputState = &vertexInputCreateInfo;

   //input assembly
   VkPipelineInputAssemblyStateCreateInfo inputAssemblyCreateInfo = {};
   inputAssemblyCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
   inputAssemblyCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
   inputAssemblyCreateInfo.primitiveRestartEnable = VK_FALSE;
   createInfo.pInputAssemblyState = &inputAssemblyCreateInfo;

   //view port and scissor
   VkViewport viewport = {};
   viewport.x = 0;
   viewport.y = 0;
   viewport.width = static_cast<float>(currentResolution.width);
   viewport.height = static_cast<float>(currentResolution.height);
   viewport.minDepth = 0.0f;
   viewport.maxDepth = 1.0f;

   VkRect2D scissor = {};
   scissor.offset = { 0, 0 };
   scissor.extent = currentResolution;

   VkPipelineViewportStateCreateInfo viewportScissorCreateInfo = {};
   viewportScissorCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
   viewportScissorCreateInfo.pViewports = &viewport;
   viewportScissorCreateInfo.viewportCount = 1;
   viewportScissorCreateInfo.pScissors = &scissor;
   viewportScissorCreateInfo.scissorCount = 1;
   createInfo.pViewportState = &viewportScissorCreateInfo;

   //dynamic state
   std::vector<VkDynamicState> dynamicStates = {};
   VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = {};
   dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
   dynamicStateCreateInfo.pDynamicStates = dynamicStates.data();
   dynamicStateCreateInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
   createInfo.pDynamicState = &dynamicStateCreateInfo;

   //resterizer
   VkPipelineRasterizationStateCreateInfo rasterCreateInfo = {};
   rasterCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
   rasterCreateInfo.depthClampEnable = VK_FALSE;
   rasterCreateInfo.rasterizerDiscardEnable = VK_FALSE;
   rasterCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
   rasterCreateInfo.lineWidth = 1.0f;
   rasterCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
   rasterCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
   rasterCreateInfo.depthBiasEnable = VK_FALSE;
   createInfo.pRasterizationState = &rasterCreateInfo;

   //multisampling
   VkPipelineMultisampleStateCreateInfo multisampleCreateInfo = {};
   multisampleCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
   multisampleCreateInfo.sampleShadingEnable = VK_FALSE;
   multisampleCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
   createInfo.pMultisampleState = &multisampleCreateInfo;

   //blending
   VkPipelineColorBlendStateCreateInfo blendCreateInfo = {};
   blendCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
   blendCreateInfo.logicOpEnable = VK_FALSE;

   VkPipelineColorBlendAttachmentState colorState = {};
   colorState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
   colorState.blendEnable = true;
   colorState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
   colorState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
   colorState.colorBlendOp = VK_BLEND_OP_ADD;
   colorState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
   colorState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
   colorState.alphaBlendOp = VK_BLEND_OP_ADD;

   blendCreateInfo.pAttachments = &colorState;
   blendCreateInfo.attachmentCount = 1;

   createInfo.pColorBlendState = &blendCreateInfo;

   //layout
   VkDescriptorSetLayout layouts[] = { subPassADescriptorSetLayout, samplerDescriptorSetLayout };

   VkPipelineLayoutCreateInfo layoutCreateInfo = {};
   layoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
   layoutCreateInfo.pSetLayouts = layouts; //sets in the shader
   layoutCreateInfo.setLayoutCount = 2; //number of sets in the shader

   VkPushConstantRange pushConstantRange = {};
   pushConstantRange.offset = 0;
   pushConstantRange.size = sizeof(UboModel);
   pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

   layoutCreateInfo.pPushConstantRanges = &pushConstantRange;
   layoutCreateInfo.pushConstantRangeCount = 1;

   if (VK_SUCCESS != vkCreatePipelineLayout(mainDevice.logicalDevice, &layoutCreateInfo, nullptr, &subPassAPipelineLayout))
      throw std::runtime_error("Unable to create pipeline layout");

   createInfo.layout = subPassAPipelineLayout;

   //render pass only one subpass per pipeline
   createInfo.renderPass = renderPass;
   createInfo.subpass = 0;

   VkPipelineDepthStencilStateCreateInfo depthStencileCreateInfo = {};
   depthStencileCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
   depthStencileCreateInfo.depthTestEnable = VK_TRUE;
   depthStencileCreateInfo.depthWriteEnable = VK_TRUE;
   depthStencileCreateInfo.depthCompareOp = VK_COMPARE_OP_LESS;
   depthStencileCreateInfo.depthBoundsTestEnable = VK_FALSE;
   depthStencileCreateInfo.stencilTestEnable = VK_FALSE;
   createInfo.pDepthStencilState = &depthStencileCreateInfo;


   //base
   createInfo.basePipelineHandle = nullptr; //pointer to a pass, only override values in the new pass
   createInfo.basePipelineIndex = -1;

   if (VK_SUCCESS != vkCreateGraphicsPipelines(mainDevice.logicalDevice, nullptr, 1, &createInfo, nullptr, &subPassAGraphicsPipeline))
      throw std::runtime_error("Failed to create pipeline");

   vkDestroyShaderModule(mainDevice.logicalDevice, vertexShaderModule, nullptr);
   vkDestroyShaderModule(mainDevice.logicalDevice, fragmentShaderModule, nullptr);

   //render pass B

   VkGraphicsPipelineCreateInfo createInfoPipelinB = createInfo;

   //shader modules
   std::vector<char> secondVertexShader = readFile("second.vert.spv");
   std::vector<char> secondFragmentShader = readFile("second.frag.spv");

   VkShaderModule secondVertexShaderModule = createShaderModule(mainDevice.logicalDevice, secondVertexShader);
   VkShaderModule secondFragmentShaderModule = createShaderModule(mainDevice.logicalDevice, secondFragmentShader);

   vertexShaderCreateInfo.module = secondVertexShaderModule;
   fragmentShaderCreateInfo.module = secondFragmentShaderModule;
   VkPipelineShaderStageCreateInfo secondShaderStages[] = { vertexShaderCreateInfo, fragmentShaderCreateInfo };
   createInfoPipelinB.pStages = secondShaderStages;
   createInfoPipelinB.stageCount = 2;

   vertexInputCreateInfo.vertexBindingDescriptionCount = 0;
   vertexInputCreateInfo.pVertexBindingDescriptions = nullptr;
   vertexInputCreateInfo.vertexAttributeDescriptionCount = 0;
   vertexInputCreateInfo.pVertexAttributeDescriptions = nullptr;
   createInfoPipelinB.pVertexInputState = &vertexInputCreateInfo;
   
   depthStencileCreateInfo.depthWriteEnable = VK_FALSE;
   createInfoPipelinB.pDepthStencilState = &depthStencileCreateInfo;

   VkPushConstantRange pipelineBPushConstants = {};
   pipelineBPushConstants.offset = 0;
   pipelineBPushConstants.size = sizeof(float);
   pipelineBPushConstants.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

   VkPipelineLayoutCreateInfo pipelineBpipelineLayoutCreateInfo = {};
   pipelineBpipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
   pipelineBpipelineLayoutCreateInfo.pPushConstantRanges = &pipelineBPushConstants;
   pipelineBpipelineLayoutCreateInfo.pushConstantRangeCount = 1;
   pipelineBpipelineLayoutCreateInfo.pSetLayouts = &subPassBDescriptorSetLayout;
   pipelineBpipelineLayoutCreateInfo.setLayoutCount = 1;
   
   if (VK_SUCCESS != vkCreatePipelineLayout(mainDevice.logicalDevice, &pipelineBpipelineLayoutCreateInfo, nullptr, &subPassBPipelineLayout))
      throw std::runtime_error("Unable to create pipeline layout");

   createInfoPipelinB.layout = subPassBPipelineLayout;

   createInfoPipelinB.subpass = 1;

   if (VK_SUCCESS != vkCreateGraphicsPipelines(mainDevice.logicalDevice, nullptr, 1, &createInfoPipelinB, nullptr, &subPassBGraphicsPipeline))
      throw std::runtime_error("Failed to create pipeline");

   vkDestroyShaderModule(mainDevice.logicalDevice, secondVertexShaderModule, nullptr);
   vkDestroyShaderModule(mainDevice.logicalDevice, secondFragmentShaderModule, nullptr);
}

void VulkanRenderer::createRenderPass()
{
   VkRenderPassCreateInfo createInfo = {};
   createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;

   VkAttachmentDescription colorAttachmentSubpass1 = {};
   colorAttachmentSubpass1.format = colorBufferFormat;
   colorAttachmentSubpass1.samples = VK_SAMPLE_COUNT_1_BIT;
   colorAttachmentSubpass1.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
   colorAttachmentSubpass1.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE; // this is what happenes after the render pass
   colorAttachmentSubpass1.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
   colorAttachmentSubpass1.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
   colorAttachmentSubpass1.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
   colorAttachmentSubpass1.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; //we don't care, this is after the render pass ends

   VkAttachmentDescription depthAttachmentsSubpass1 = {};
   depthAttachmentsSubpass1.format = depthBufferFormat;
   depthAttachmentsSubpass1.samples = VK_SAMPLE_COUNT_1_BIT;
   depthAttachmentsSubpass1.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
   depthAttachmentsSubpass1.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE; // we only use this when we draw
   depthAttachmentsSubpass1.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
   depthAttachmentsSubpass1.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
   depthAttachmentsSubpass1.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
   depthAttachmentsSubpass1.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

   //color attachment
   VkAttachmentDescription colorAttachmentSubpass2 = {};
   colorAttachmentSubpass2.format = currentSurfaceFormat.format;
   colorAttachmentSubpass2.samples = VK_SAMPLE_COUNT_1_BIT;
   colorAttachmentSubpass2.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
   colorAttachmentSubpass2.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
   colorAttachmentSubpass2.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
   colorAttachmentSubpass2.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
   colorAttachmentSubpass2.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
   colorAttachmentSubpass2.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;


   VkAttachmentDescription attachments[] = { colorAttachmentSubpass1, depthAttachmentsSubpass1, colorAttachmentSubpass2 };
   createInfo.pAttachments = attachments;
   createInfo.attachmentCount = 3;

   //subpass a
   VkAttachmentReference colorAttachmentSubpass1Reference = {};
   colorAttachmentSubpass1Reference.attachment = 0; // createInfo.pAttachments index
   colorAttachmentSubpass1Reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

   VkAttachmentReference depthAttachmentsSubpass1Reference = {};
   depthAttachmentsSubpass1Reference.attachment = 1;
   depthAttachmentsSubpass1Reference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

   VkSubpassDescription subpassA = {};
   subpassA.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
   subpassA.colorAttachmentCount = 1;
   subpassA.pColorAttachments = &colorAttachmentSubpass1Reference;
   subpassA.pDepthStencilAttachment = &depthAttachmentsSubpass1Reference;

   //subpass b
   VkAttachmentReference colorAttachmentSubpass2Reference = {};
   colorAttachmentSubpass2Reference.attachment = 2;
   colorAttachmentSubpass2Reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

   //input attachements subpass b
   VkAttachmentReference colorAttachmentSubpass2FromSubpass1Reference = {};
   colorAttachmentSubpass2FromSubpass1Reference.attachment = 0;
   colorAttachmentSubpass2FromSubpass1Reference.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

   VkAttachmentReference depthAttachmentSubpass2FromSubpass1Reference = {};
   depthAttachmentSubpass2FromSubpass1Reference.attachment = 1;
   depthAttachmentSubpass2FromSubpass1Reference.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

   VkAttachmentReference inputAttachmentsSubpassB[] = { colorAttachmentSubpass2FromSubpass1Reference, depthAttachmentSubpass2FromSubpass1Reference };

   VkSubpassDescription subpassB = {};
   subpassB.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
   subpassB.colorAttachmentCount = 1;
   subpassB.pColorAttachments = &colorAttachmentSubpass2Reference;
   subpassB.pDepthStencilAttachment = nullptr;
   subpassB.pInputAttachments = inputAttachmentsSubpassB;
   subpassB.inputAttachmentCount = 2;

   VkSubpassDescription subpasses[] = { subpassA, subpassB };
   createInfo.subpassCount = 2;
   createInfo.pSubpasses = subpasses;

   //dependencies
   std::vector<VkSubpassDependency> subpassDependencies(3);

   //VK_IMAGE_LAYOUT_UNDEFINED to VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL >

   //start conversion after VK_SUBPASS_EXTERNAL finised reading and at VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT
   subpassDependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
   subpassDependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
   subpassDependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;

   //end conversion when pass 0 finished all read and write ops before VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
   subpassDependencies[0].dstSubpass = 0; //indx in createInfo.pSubpasses
   subpassDependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
   subpassDependencies[0].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;

   //subpass A -> subpassB
   subpassDependencies[1].srcSubpass = 0;
   subpassDependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT; //start conversion when subpassA stops writeing in the framebuffer
   subpassDependencies[1].srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;

   subpassDependencies[1].dstSubpass = 1; //indx in createInfo.pSubpasses
   subpassDependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT; //end conversion before subpassB starts to readin the fragment shader
   subpassDependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

   //VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL to VK_IMAGE_LAYOUT_PRESENT_SRC_KHR

   //start conversion after pass 0 finised reading and at VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
   subpassDependencies[2].srcSubpass = 1;
   subpassDependencies[2].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
   subpassDependencies[2].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;

   //end conversion when VK_SUBPASS_EXTERNAL all read and write ops before VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT
   subpassDependencies[2].dstSubpass = VK_SUBPASS_EXTERNAL;
   subpassDependencies[2].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
   subpassDependencies[2].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;

   createInfo.pDependencies = subpassDependencies.data();
   createInfo.dependencyCount = static_cast<uint32_t>(subpassDependencies.size());

   if (VK_SUCCESS != vkCreateRenderPass(mainDevice.logicalDevice, &createInfo, nullptr, &renderPass))
      throw std::runtime_error("Failed to create render pass");
}

void VulkanRenderer::createFrameBuffers()
{
   swapChainFramebuffers.resize(swapChainImages.size());

   for (size_t i = 0; i < swapChainFramebuffers.size(); ++i)
   {
      std::vector<VkImageView> attachments;
      attachments.push_back(colorBuffers[i].imageView); //same order as in the render pass
      attachments.push_back(depthBuffers[i].imageView);
      attachments.push_back(swapChainImages[i].imageView);

      VkFramebufferCreateInfo createInfo = {};
      createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
      createInfo.renderPass = renderPass;
      createInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
      createInfo.pAttachments = attachments.data();
      createInfo.width = currentResolution.width;
      createInfo.height = currentResolution.height;
      createInfo.layers = 1;

      if (VK_SUCCESS != vkCreateFramebuffer(mainDevice.logicalDevice, &createInfo, nullptr, &swapChainFramebuffers[i]))
         throw std::runtime_error("Unable to create framebuffer");
   }
}

void VulkanRenderer::createCommandPool()
{
   VkCommandPoolCreateInfo createInfo = {};
   createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
   createInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
   createInfo.queueFamilyIndex = queueFamilyIndices.graphicFamily;
   
   if (VK_SUCCESS != vkCreateCommandPool(mainDevice.logicalDevice, &createInfo, nullptr, &graphicsCommandPool))
      throw std::runtime_error("Unable to create the command pool");
}

void VulkanRenderer::allocateCommandBuffers()
{
   commandBuffers.resize(swapChainFramebuffers.size());

   VkCommandBufferAllocateInfo allocInfo = {};
   allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
   allocInfo.commandPool = graphicsCommandPool;
   allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
   allocInfo.commandBufferCount = static_cast<uint32_t>(swapChainFramebuffers.size());

   if (VK_SUCCESS != vkAllocateCommandBuffers(mainDevice.logicalDevice, &allocInfo, commandBuffers.data()))
      throw std::runtime_error("Failed to allocate command buffers");
}

void VulkanRenderer::createSyncronization()
{
   VkSemaphoreCreateInfo semaphoreCreateInfo = {};
   semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

   VkFenceCreateInfo fenceCreateInfo = {};
   fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
   fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

   imagesAvailable.resize(MAX_NUMBER_OF_PROCCESSED_FRAMES_INFLIGHT);
   rendersFinished.resize(MAX_NUMBER_OF_PROCCESSED_FRAMES_INFLIGHT);
   for (size_t i = 0; i < MAX_NUMBER_OF_PROCCESSED_FRAMES_INFLIGHT; ++i)
   {
      if (VK_SUCCESS != vkCreateSemaphore(mainDevice.logicalDevice, &semaphoreCreateInfo, nullptr, &imagesAvailable[i]))
         throw std::runtime_error("Unable to create image semaphore");

      if (VK_SUCCESS != vkCreateSemaphore(mainDevice.logicalDevice, &semaphoreCreateInfo, nullptr, &rendersFinished[i]))
         throw std::runtime_error("Unable to create render semaphore");
   }

   drawFences.resize(MAX_NUMBER_OF_PROCCESSED_FRAMES_INFLIGHT);
   for (size_t i = 0; i < MAX_NUMBER_OF_PROCCESSED_FRAMES_INFLIGHT; ++i)
   {
      if (VK_SUCCESS != vkCreateFence(mainDevice.logicalDevice, &fenceCreateInfo, nullptr, &drawFences[i]))
         throw std::runtime_error("Unable to create fence");
   }
}

void VulkanRenderer::createSubPassASamplerDescriptorSetLayout()
{
   VkDescriptorSetLayoutBinding samplerBinding = {};
   samplerBinding.binding = 0;//shader binding
   samplerBinding.descriptorCount = 1;
   samplerBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
   samplerBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;//stage where to bind

   VkDescriptorSetLayoutCreateInfo createInfo = {};
   createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
   createInfo.bindingCount = 1;
   createInfo.pBindings = &samplerBinding;

   if (VK_SUCCESS != vkCreateDescriptorSetLayout(mainDevice.logicalDevice, &createInfo, nullptr, &samplerDescriptorSetLayout))
      throw std::runtime_error("Unable to create sampler descriptor set layout");
}

void VulkanRenderer::createSubPassADescriptorSetLayout()
{
   VkDescriptorSetLayoutBinding vpBinding = {};
   vpBinding.binding = 0;//shader binding
   vpBinding.descriptorCount = 1;
   vpBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
   vpBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;//stage where to bind

   VkDescriptorSetLayoutBinding modelBinding = {};
   modelBinding.binding = 1;//shader binding
   modelBinding.descriptorCount = 1;
   modelBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
   modelBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;//stage where to bind

   VkDescriptorSetLayoutBinding bindings[] = { vpBinding, modelBinding };

   VkDescriptorSetLayoutCreateInfo createInfo = {};
   createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
   createInfo.bindingCount = 2;
   createInfo.pBindings = bindings;

   if (VK_SUCCESS != vkCreateDescriptorSetLayout(mainDevice.logicalDevice, &createInfo, nullptr, &subPassADescriptorSetLayout))
      throw std::runtime_error("Unable to create descriptor set layout");

}

void VulkanRenderer::createSubPassBDescriptorSetLayout()
{
   VkDescriptorSetLayoutBinding colorInputLayoutBinding = {};
   colorInputLayoutBinding.binding = 0;
   colorInputLayoutBinding.descriptorCount = 1;
   colorInputLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
   colorInputLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

   VkDescriptorSetLayoutBinding depthInputLayoutBinding = {};
   depthInputLayoutBinding.binding = 1;
   depthInputLayoutBinding.descriptorCount = 1;
   depthInputLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
   depthInputLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

   VkDescriptorSetLayoutBinding bindings[] = { colorInputLayoutBinding, depthInputLayoutBinding };

   VkDescriptorSetLayoutCreateInfo createInfo = {};
   createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
   createInfo.bindingCount = 2;
   createInfo.pBindings = bindings;

   if (VK_SUCCESS != vkCreateDescriptorSetLayout(mainDevice.logicalDevice, &createInfo, nullptr, &subPassBDescriptorSetLayout))
      throw std::runtime_error("Unable to create descriptor set layout");
}

void VulkanRenderer::createSamplerDescriptorPool()
{
   VkDescriptorPoolSize samplerPoolSize = {};
   samplerPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
   samplerPoolSize.descriptorCount = static_cast<uint32_t>(MAX_TEXTURES); //nr of descriptors, not sets

   VkDescriptorPoolCreateInfo poolCreateInfo = {};
   poolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
   poolCreateInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
   poolCreateInfo.maxSets = static_cast<uint32_t>(MAX_TEXTURES);
   poolCreateInfo.poolSizeCount = 1;
   poolCreateInfo.pPoolSizes = &samplerPoolSize;
   if (VK_SUCCESS != vkCreateDescriptorPool(mainDevice.logicalDevice, &poolCreateInfo, nullptr, &subPassASamplerDescriptorPool))
      throw std::runtime_error("Unable to create descriptor set pool");
}

void VulkanRenderer::crateSubPassABufferDescriptorSetPool()
{
   VkDescriptorPoolSize uboPoolSize = {};
   uboPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
   uboPoolSize.descriptorCount = static_cast<uint32_t>(uboBuffers.size()); //nr of descriptors, not sets

   VkDescriptorPoolSize dynamicUboPoolSize = {};
   dynamicUboPoolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
   dynamicUboPoolSize.descriptorCount = static_cast<uint32_t>(dynamicUboBuffers.size());

   VkDescriptorPoolSize renderPassAPoolSizes[] = { uboPoolSize , dynamicUboPoolSize };

   VkDescriptorPoolCreateInfo renderPassAPoolCreateInfo = {};
   renderPassAPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
   renderPassAPoolCreateInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
   renderPassAPoolCreateInfo.maxSets = static_cast<uint32_t>(uboBuffers.size()) + static_cast<uint32_t>(dynamicUboBuffers.size());
   renderPassAPoolCreateInfo.poolSizeCount = 2;
   renderPassAPoolCreateInfo.pPoolSizes = renderPassAPoolSizes;
   if (VK_SUCCESS != vkCreateDescriptorPool(mainDevice.logicalDevice, &renderPassAPoolCreateInfo, nullptr, &subPassABufferDescriptorPool))
      throw std::runtime_error("Unable to create descriptor set pool");
}

void VulkanRenderer::crateSubPassBInputDescriptorSetPool()
{
   VkDescriptorPoolSize colorInputPoolSize = {};
   colorInputPoolSize.type = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
   colorInputPoolSize.descriptorCount = static_cast<uint32_t>(colorBuffers.size());

   VkDescriptorPoolSize depthInputPoolSize = {};
   depthInputPoolSize.type = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
   depthInputPoolSize.descriptorCount = static_cast<uint32_t>(depthBuffers.size());

   VkDescriptorPoolSize renderPassBPoolSizes[] = { colorInputPoolSize , depthInputPoolSize };

   VkDescriptorPoolCreateInfo renderPassBPoolCreateInfo = {};
   renderPassBPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
   renderPassBPoolCreateInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
   renderPassBPoolCreateInfo.maxSets = static_cast<uint32_t>(colorBuffers.size()) + static_cast<uint32_t>(depthBuffers.size());
   renderPassBPoolCreateInfo.poolSizeCount = 2;
   renderPassBPoolCreateInfo.pPoolSizes = renderPassBPoolSizes;
   if (VK_SUCCESS != vkCreateDescriptorPool(mainDevice.logicalDevice, &renderPassBPoolCreateInfo, nullptr, &subPassBInputsDescriptorPool))
      throw std::runtime_error("Unable to create descriptor set pool");
}

void VulkanRenderer::createSubPassABufferDescriptorSet()
{
   std::vector<VkDescriptorSetLayout> layouts(std::max(uboBuffers.size(), dynamicUboBuffers.size()), subPassADescriptorSetLayout); //one per allocated set !

   //ubo descriptor set allocation
   {
      VkDescriptorSetAllocateInfo descriptorSetAllocationInfo = {};
      descriptorSetAllocationInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
      descriptorSetAllocationInfo.descriptorPool = subPassABufferDescriptorPool;
      descriptorSetAllocationInfo.descriptorSetCount = static_cast<uint32_t>(uboBuffers.size());
      descriptorSetAllocationInfo.pSetLayouts = layouts.data();

      subPassABufferDescriptorSets.resize(static_cast<uint32_t>(uboBuffers.size()));
      if (VK_SUCCESS != vkAllocateDescriptorSets(mainDevice.logicalDevice, &descriptorSetAllocationInfo, subPassABufferDescriptorSets.data()))
         throw std::runtime_error("Unable to allocate descriptors for ubo");

      //bind the buffers to descriptors
      for (size_t i = 0; i < uboBuffers.size(); ++i)
      {
         VkDescriptorBufferInfo uboBufferInfo = {};
         uboBufferInfo.buffer = uboBuffers[i];
         uboBufferInfo.offset = 0;
         uboBufferInfo.range = sizeof(UboViewProjection);

         VkWriteDescriptorSet mvpDescriptorSet = {};
         mvpDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
         mvpDescriptorSet.dstSet = subPassABufferDescriptorSets[i];
         mvpDescriptorSet.dstBinding = 0; //binding from layout or shader
         mvpDescriptorSet.dstArrayElement = 0; //index if this is an array
         mvpDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
         mvpDescriptorSet.descriptorCount = 1;
         mvpDescriptorSet.pBufferInfo = &uboBufferInfo;


         VkDescriptorBufferInfo dynamicBufferInfo = {};
         dynamicBufferInfo.buffer = dynamicUboBuffers[i];
         dynamicBufferInfo.offset = 0;
         dynamicBufferInfo.range = modelUniformAlignment; // for dynamic object we pass the size of an abject not of the whole buffer

         VkWriteDescriptorSet modelDescriptorSet = {};
         modelDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
         modelDescriptorSet.dstSet = subPassABufferDescriptorSets[i];
         modelDescriptorSet.dstBinding = 1; //binding from layout or shader
         modelDescriptorSet.dstArrayElement = 0;
         modelDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
         modelDescriptorSet.descriptorCount = 1;
         modelDescriptorSet.pBufferInfo = &dynamicBufferInfo;

         VkWriteDescriptorSet setWrites[] = { mvpDescriptorSet, modelDescriptorSet };

         vkUpdateDescriptorSets(mainDevice.logicalDevice, 2, setWrites, 0, nullptr);
      }
   }
}

void VulkanRenderer::createSubPassBInputDescriptorSet()
{
   std::vector<VkDescriptorSetLayout> layouts(colorBuffers.size(), subPassBDescriptorSetLayout); //one per allocated set !

   VkDescriptorSetAllocateInfo descriptorSetAllocationInfo = {};
   descriptorSetAllocationInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
   descriptorSetAllocationInfo.descriptorPool = subPassBInputsDescriptorPool;
   descriptorSetAllocationInfo.descriptorSetCount = static_cast<uint32_t>(colorBuffers.size());
   descriptorSetAllocationInfo.pSetLayouts = layouts.data();

   subPassBInputDescriptorSets.resize(static_cast<uint32_t>(uboBuffers.size()));
   if (VK_SUCCESS != vkAllocateDescriptorSets(mainDevice.logicalDevice, &descriptorSetAllocationInfo, subPassBInputDescriptorSets.data()))
      throw std::runtime_error("Unable to allocate descriptors for inputs on subpass 2");

   for (size_t i = 0; i < colorBuffers.size(); ++i)
   {
      VkDescriptorImageInfo colorBufferInfo = {};
      colorBufferInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      colorBufferInfo.imageView = colorBuffers[i].imageView;
      colorBufferInfo.sampler = VK_NULL_HANDLE; //input from other subpasses can't be sampled

      VkWriteDescriptorSet colorInputDescriptorSet = {};
      colorInputDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      colorInputDescriptorSet.dstSet = subPassBInputDescriptorSets[i];
      colorInputDescriptorSet.dstBinding = 0; //binding from layout or shader
      colorInputDescriptorSet.dstArrayElement = 0; //index if this is an array
      colorInputDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
      colorInputDescriptorSet.descriptorCount = 1;
      colorInputDescriptorSet.pImageInfo = &colorBufferInfo;

      VkDescriptorImageInfo depthBufferInfo = {};
      depthBufferInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      depthBufferInfo.imageView = depthBuffers[i].imageView;
      depthBufferInfo.sampler = VK_NULL_HANDLE;

      VkWriteDescriptorSet depthInputDescriptorSet = {};
      depthInputDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      depthInputDescriptorSet.dstSet = subPassBInputDescriptorSets[i];
      depthInputDescriptorSet.dstBinding = 1;
      depthInputDescriptorSet.dstArrayElement = 0;
      depthInputDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
      depthInputDescriptorSet.descriptorCount = 1;
      depthInputDescriptorSet.pImageInfo = &depthBufferInfo;

      VkWriteDescriptorSet setWrites[] = { colorInputDescriptorSet, depthInputDescriptorSet };

      vkUpdateDescriptorSets(mainDevice.logicalDevice, 2, setWrites, 0, nullptr);
   }
}

void VulkanRenderer::createUniformBuffers()
{
   //uniform buffer objects
   for (size_t i = 0; i < swapChainImages.size(); ++i)
   {
      VkBuffer out = VK_NULL_HANDLE;
      VkDeviceMemory outMemory = VK_NULL_HANDLE;
      creteBuffer(mainDevice.physicalDevice, mainDevice.logicalDevice, sizeof(UboViewProjection),
         VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
         VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
         &out, &outMemory);

      uboBuffers.push_back(out);
      uboBuffersMemory.push_back(outMemory);
   }

   //dynamic uniform buffer objects
   for (size_t i = 0; i < swapChainImages.size(); ++i)
   {
      VkBuffer out = VK_NULL_HANDLE;
      VkDeviceMemory outMemory = VK_NULL_HANDLE;
      creteBuffer(mainDevice.physicalDevice, mainDevice.logicalDevice, modelUniformAlignment * MAX_OBJECTS,
         VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
         VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
         &out, &outMemory);

      dynamicUboBuffers.push_back(out);
      dynamicUboBuffersMemory.push_back(outMemory);
   }
}

void VulkanRenderer::createDepthBuffer()
{
   depthBuffers.resize(swapChainImages.size());
   for (auto& depthBuffer : depthBuffers)
   {
      depthBuffer.image = createImage(currentResolution.width, currentResolution.height, depthBufferFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &depthBuffer.deviceMemory);

      depthBuffer.imageView = createImageView(mainDevice.logicalDevice, depthBuffer.image, depthBufferFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
   }
}

void VulkanRenderer::createColorBuffer()
{
   colorBuffers.resize(swapChainImages.size());
   for (auto& colorBuffer : colorBuffers)
   {
      colorBuffer.image = createImage(currentResolution.width, currentResolution.height, colorBufferFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &colorBuffer.deviceMemory);

      colorBuffer.imageView = createImageView(mainDevice.logicalDevice, colorBuffer.image, colorBufferFormat, VK_IMAGE_ASPECT_COLOR_BIT);
   }
}

VkFormat VulkanRenderer::choseOptimalImageFormat(const std::vector<VkFormat> formats, VkImageTiling tiling, VkFormatFeatureFlags flags) const
{
   for (auto& format : formats)
   {
      VkFormatProperties properties = {};
      vkGetPhysicalDeviceFormatProperties(mainDevice.physicalDevice, format, &properties);

      VkFormatFeatureFlags currentFlags = properties.linearTilingFeatures;
      if (tiling == VK_IMAGE_TILING_OPTIMAL)
         currentFlags = properties.optimalTilingFeatures;

      if ((currentFlags & flags) == flags)
         return format;
   }
   throw std::runtime_error("Failed to find a maching format");
}

VkImage VulkanRenderer::createImage(uint32_t width, uint32_t height, VkFormat format,
   VkImageTiling tiling, VkImageUsageFlags usageFlags, VkMemoryPropertyFlags propertyFlags, VkDeviceMemory* imageMemory) const
{
   VkImageCreateInfo imageCreateInfo = {};
   imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
   imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
   imageCreateInfo.extent = { width, height, 1 };
   imageCreateInfo.mipLevels = 1;
   imageCreateInfo.arrayLayers = 1;
   imageCreateInfo.format = format;
   imageCreateInfo.tiling = tiling;
   imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
   imageCreateInfo.usage = usageFlags;
   imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
   imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

   VkImage image = VK_NULL_HANDLE;
   if (VK_SUCCESS != vkCreateImage(mainDevice.logicalDevice, &imageCreateInfo, nullptr, &image))
      throw std::runtime_error("Unable to create image");

   VkMemoryRequirements imageMemoryRequierments = {};
   vkGetImageMemoryRequirements(mainDevice.logicalDevice, image, &imageMemoryRequierments);

   VkMemoryAllocateInfo memoryAllocateInfo = {};
   memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
   memoryAllocateInfo.memoryTypeIndex = findMemoryTypeIndex(mainDevice.physicalDevice, imageMemoryRequierments.memoryTypeBits, propertyFlags);
   memoryAllocateInfo.allocationSize = imageMemoryRequierments.size;
   
   if (VK_SUCCESS != vkAllocateMemory(mainDevice.logicalDevice, &memoryAllocateInfo, nullptr, imageMemory))
      throw std::runtime_error("Unable to allocate image memory");

   if (VK_SUCCESS != vkBindImageMemory(mainDevice.logicalDevice, image, *imageMemory, 0))
      throw std::runtime_error("Unable to allocate image memory");

   return image;
}

void VulkanRenderer::updateUniformBuffers(size_t frame)
{
   //update ubo
   void* outData = nullptr;
   if (VK_SUCCESS != vkMapMemory(mainDevice.logicalDevice, uboBuffersMemory[frame], 0, sizeof(UboViewProjection), 0, &outData))
      throw std::runtime_error("Unable to map ubo buffer");

   memcpy(outData, &uboViewProjection, sizeof(UboViewProjection));

   vkUnmapMemory(mainDevice.logicalDevice, uboBuffersMemory[frame]);
   outData = nullptr;

   //update dynamic uniform buffers object, in the drawing order
   size_t meshaesCount = 0;
   for (size_t i = 0; i < meshes.size(); ++i)
   {
      UboModel uboModel;
      uboModel.model = meshes[i].getModel();

      for (size_t m = 0; m < meshes[i].getMeshCount(); ++m)
      {
         if (meshaesCount > MAX_OBJECTS)
            return;

         UboModel* allignedLocation = reinterpret_cast<UboModel*>(reinterpret_cast<char*>(modelTransferSpace) + meshaesCount * modelUniformAlignment);
         memcpy(allignedLocation, &uboModel, sizeof(UboModel));

         ++meshaesCount;
      }
   }

   void* bufferMem = nullptr;
   VkDeviceSize size = modelUniformAlignment * std::min(MAX_OBJECTS, meshes.size());
   if (VK_SUCCESS != vkMapMemory(mainDevice.logicalDevice, dynamicUboBuffersMemory[frame], 0, size, 0, &bufferMem))
      throw std::runtime_error("Unable to map dynamic object memory");
   memcpy(bufferMem, modelTransferSpace, size);
   vkUnmapMemory(mainDevice.logicalDevice, dynamicUboBuffersMemory[frame]);
}

void VulkanRenderer::allocateDynamicBufferTransferSpace()
{
   modelUniformAlignment = (sizeof(UboModel) + mainDevice.minStorageBufferOffsetAlignment - 1) 
      & ~(mainDevice.minStorageBufferOffsetAlignment - 1);

   modelTransferSpace = reinterpret_cast<UboModel*>(_aligned_malloc(MAX_OBJECTS * modelUniformAlignment, modelUniformAlignment));
}

uint32_t VulkanRenderer::loadTexture(const char* imageFileName)
{
   for (uint32_t i = 0; i < loadedTextures.size(); ++i)
      if (loadedTextures[i].fileName == imageFileName)
         return i;

   Image i = readImage(imageFileName, ReadImageChannels::rgb_alpha);

   if (i.data.empty())
      throw std::runtime_error("Could not load texture");

   VkBuffer stagingBuffer = VK_NULL_HANDLE;
   VkDeviceMemory stagingMemory = VK_NULL_HANDLE;

   creteBuffer(mainDevice.physicalDevice, mainDevice.logicalDevice, i.data.size(),
      VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, &stagingBuffer, &stagingMemory);

   void* data = nullptr;
   if (VK_SUCCESS != vkMapMemory(mainDevice.logicalDevice, stagingMemory, 0, i.data.size(), 0, &data))
      throw std::runtime_error("Unable to map staging buffer for image");

   memcpy(data, i.data.data(), i.data.size());

   vkUnmapMemory(mainDevice.logicalDevice, stagingMemory);

   VkFormat imageFormat = VK_FORMAT_R8G8B8A8_UNORM;

   VkDeviceMemory outMemory = VK_NULL_HANDLE;
   VkImage out = createImage(i.width, i.height, 
      imageFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
      VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &outMemory);

   transitionImageLayout(mainDevice.logicalDevice, graphicsQueue, graphicsCommandPool, out, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
   copyImage(mainDevice.logicalDevice, graphicsQueue, graphicsCommandPool, out, stagingBuffer, i.width, i.height);
   transitionImageLayout(mainDevice.logicalDevice, graphicsQueue, graphicsCommandPool, out, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

   VkImageView outImageView = createImageView(mainDevice.logicalDevice, out, imageFormat, VK_IMAGE_ASPECT_COLOR_BIT);

   vkFreeMemory(mainDevice.logicalDevice, stagingMemory, nullptr);
   vkDestroyBuffer(mainDevice.logicalDevice, stagingBuffer, nullptr);

   VkDescriptorSet outSet = VK_NULL_HANDLE;
   {
      VkDescriptorSetAllocateInfo descriptorSetAllocationInfo = {};
      descriptorSetAllocationInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
      descriptorSetAllocationInfo.descriptorPool = subPassASamplerDescriptorPool;
      descriptorSetAllocationInfo.descriptorSetCount = 1;
      descriptorSetAllocationInfo.pSetLayouts = &samplerDescriptorSetLayout;

      if (VK_SUCCESS != vkAllocateDescriptorSets(mainDevice.logicalDevice, &descriptorSetAllocationInfo, &outSet))
         throw std::runtime_error("Unable to allocate descriptors for samplers");

      VkDescriptorImageInfo imageInfo = {};
      imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      imageInfo.imageView = outImageView;
      imageInfo.sampler = textureSampler;

      VkWriteDescriptorSet writeDescriptorSet = {};
      writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      writeDescriptorSet.descriptorCount = 1;
      writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
      writeDescriptorSet.dstBinding = 0;
      writeDescriptorSet.dstArrayElement = 0;
      writeDescriptorSet.pImageInfo = &imageInfo;
      writeDescriptorSet.dstSet = outSet;
      vkUpdateDescriptorSets(mainDevice.logicalDevice, 1, &writeDescriptorSet, 0, nullptr);
   }

   LoadedImage li{ imageFileName, out, outMemory, outImageView, outSet };
   loadedTextures.emplace_back(std::move(li));

   return static_cast<uint32_t>(loadedTextures.size() - 1);
}

void VulkanRenderer::createTextureSampler()
{
   VkSamplerCreateInfo createInfo = {};
   createInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
   createInfo.magFilter = VK_FILTER_LINEAR;
   createInfo.minFilter = VK_FILTER_LINEAR;
   createInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
   createInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
   createInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
   createInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
   createInfo.minFilter = VK_FILTER_LINEAR;
   createInfo.mipLodBias = 0.0f;
   createInfo.minLod = 0;
   createInfo.maxLod = 0;
   createInfo.anisotropyEnable = VK_FALSE;
   createInfo.compareEnable = VK_FALSE;
   createInfo.unnormalizedCoordinates = VK_FALSE;

   if (VK_SUCCESS != vkCreateSampler(mainDevice.logicalDevice, &createInfo, nullptr, &textureSampler))
      throw std::runtime_error("Unable to create texture sampler");
}

void VulkanRenderer::recordCommandBuffers(size_t frame)
{
   VkCommandBufferBeginInfo beginInfo = {};
   beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;


   if (VK_SUCCESS != vkBeginCommandBuffer(commandBuffers[frame], &beginInfo))
      throw std::runtime_error("Unable to begin recording command buffer");

   VkRenderPassBeginInfo beginRenderPassInfo = {};
   beginRenderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
   beginRenderPassInfo.renderPass = renderPass;
   beginRenderPassInfo.renderArea.offset = { 0,0 };
   beginRenderPassInfo.renderArea.extent = currentResolution;
   VkClearValue clearValues[] = {
      { 3.0f / 255.0f, 131.0f / 255.0f, 135.0f / 255.0f, 1.0f },
      {1.0f},
      {}
   };
   beginRenderPassInfo.pClearValues = clearValues;
   beginRenderPassInfo.clearValueCount = 3;
   beginRenderPassInfo.framebuffer = swapChainFramebuffers[frame];


   vkCmdBeginRenderPass(commandBuffers[frame], &beginRenderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

   //render subpass A
   vkCmdBindPipeline(commandBuffers[frame], VK_PIPELINE_BIND_POINT_GRAPHICS, subPassAGraphicsPipeline);

   uint32_t meshIndex = 0;
   for (auto& model: meshes)
   {
      vkCmdPushConstants(commandBuffers[frame], subPassAPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushModel), &model.getPushData());

      for (uint32_t m = 0; m < model.getMeshCount(); ++m) {
         VkDescriptorSet descriptors[] = { subPassABufferDescriptorSets[frame], loadedTextures[model.getMesh(m)->getTextureId()].samplerSet };

         uint32_t dynamicOffset = static_cast<uint32_t>(modelUniformAlignment) * meshIndex++;
         vkCmdBindDescriptorSets(commandBuffers[frame], VK_PIPELINE_BIND_POINT_GRAPHICS, subPassAPipelineLayout, 0, 2, descriptors, 1, &dynamicOffset);

         VkDeviceSize offsets[] = { 0 };
         VkBuffer buffers[] = { model.getMesh(m)->getVertexBuffer() };
         vkCmdBindVertexBuffers(commandBuffers[frame], 0, 1, buffers, offsets);

         vkCmdBindIndexBuffer(commandBuffers[frame], model.getMesh(m)->getIndexBuffer(), 0, VK_INDEX_TYPE_UINT16);

         vkCmdDrawIndexed(commandBuffers[frame], model.getMesh(m)->getIndicesCount(), 1, 0, 0, 0);
      }
   }

   //render subpass B
   vkCmdNextSubpass(commandBuffers[frame], VK_SUBPASS_CONTENTS_INLINE);

   vkCmdBindPipeline(commandBuffers[frame], VK_PIPELINE_BIND_POINT_GRAPHICS, subPassBGraphicsPipeline);
   vkCmdBindDescriptorSets(commandBuffers[frame], VK_PIPELINE_BIND_POINT_GRAPHICS, subPassBPipelineLayout, 0, 1, &subPassBInputDescriptorSets[frame], 0, nullptr);

   float screenWidth = static_cast<float>(currentResolution.width);
   vkCmdPushConstants(commandBuffers[frame], subPassBPipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(float), &screenWidth);

   vkCmdDraw(commandBuffers[frame], 6, 1, 0, 0);

   vkCmdEndRenderPass(commandBuffers[frame]);

   if (VK_SUCCESS != vkEndCommandBuffer(commandBuffers[frame]))
      throw std::runtime_error("Unable to end recording command buffer");

}

VkShaderModule VulkanRenderer::createShaderModule(VkDevice device, const std::vector<char>& code) const
{
   VkShaderModuleCreateInfo createInfo = {};
   createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
   createInfo.codeSize = code.size();
   createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());
   
   VkShaderModule out = VK_NULL_HANDLE;

   if (VK_SUCCESS != vkCreateShaderModule(device, &createInfo, nullptr, &out))
      throw std::runtime_error("Unable to create shader module");

   return out;
}

VkExtent2D VulkanRenderer::selectBestResolution(GLFWwindow* window, VkSurfaceCapabilitiesKHR surfaceCapabilityes) const
{
   if (surfaceCapabilityes.currentExtent.width != UINT32_MAX && surfaceCapabilityes.currentExtent.height != UINT32_MAX)
      return surfaceCapabilityes.currentExtent;

   int width = 0;
   int height = 0;
   glfwGetWindowSize(window, &width, &height);

   uint32_t widthV = static_cast<uint32_t>(width);
   uint32_t heightV = static_cast<uint32_t>(height);

   if (widthV > surfaceCapabilityes.maxImageExtent.width)
      widthV = surfaceCapabilityes.maxImageExtent.width;

   if (heightV > surfaceCapabilityes.maxImageExtent.height)
      heightV = surfaceCapabilityes.maxImageExtent.height;

   if (widthV < surfaceCapabilityes.minImageExtent.width)
      widthV = surfaceCapabilityes.minImageExtent.width;

   if (heightV < surfaceCapabilityes.minImageExtent.height)
      heightV = surfaceCapabilityes.minImageExtent.height;

   return { widthV ,  heightV };
}

VkImageView VulkanRenderer::createImageView(VkDevice device, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags) const
{
   VkImageViewCreateInfo createInfo = {};
   createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
   createInfo.image = image;
   createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
   createInfo.format = format;
   createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
   createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
   createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
   createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

   createInfo.subresourceRange.aspectMask = aspectFlags;
   createInfo.subresourceRange.baseMipLevel = 0;
   createInfo.subresourceRange.levelCount = 1;
   createInfo.subresourceRange.baseArrayLayer = 0;
   createInfo.subresourceRange.layerCount = 1;

   VkImageView imageView = VK_NULL_HANDLE;
   if (VK_SUCCESS != vkCreateImageView(device, &createInfo, nullptr, &imageView))
      throw std::runtime_error("Unable to create image view");

   return imageView;
}

void VulkanRenderer::createSwapChain()
{
   VkSurfaceFormatKHR surfaceFormat = selectBestSurfaceFormat(swapchainDetails.supportedFormats);
   VkPresentModeKHR presentationMode = selectBestPresentationMode(swapchainDetails.supportedPresentationModes);
   VkExtent2D resolution = selectBestResolution(window, swapchainDetails.capabilities);

   uint32_t imageCount = swapchainDetails.capabilities.minImageCount + 1;
   if (imageCount > swapchainDetails.capabilities.maxImageCount)
      imageCount = swapchainDetails.capabilities.maxImageCount;

   VkSwapchainCreateInfoKHR createInfo = {};
   createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
   createInfo.surface = surface;
   createInfo.presentMode = presentationMode;
   createInfo.imageExtent = resolution;
   createInfo.imageFormat = surfaceFormat.format;
   createInfo.imageColorSpace = surfaceFormat.colorSpace;
   createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
   createInfo.preTransform = swapchainDetails.capabilities.currentTransform;
   createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
   createInfo.minImageCount = imageCount;
   createInfo.imageArrayLayers = 1;
   createInfo.clipped = VK_TRUE;

   if (queueFamilyIndices.graphicFamily != queueFamilyIndices.presentationFamily)
   {
      createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
      createInfo.queueFamilyIndexCount = 2;
      uint32_t queueIds[2] = { static_cast<uint32_t>(queueFamilyIndices.graphicFamily), static_cast<uint32_t>(queueFamilyIndices.presentationFamily) };
      createInfo.pQueueFamilyIndices = queueIds;
   }
   else
   {
      createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
      createInfo.queueFamilyIndexCount = 1;
      uint32_t queueIds[1] = { static_cast<uint32_t>(queueFamilyIndices.graphicFamily) };
      createInfo.pQueueFamilyIndices = queueIds;
   }

   //TODO: Set this on resize
   createInfo.oldSwapchain = VK_NULL_HANDLE;

   if (VK_SUCCESS != vkCreateSwapchainKHR(mainDevice.logicalDevice, &createInfo, nullptr, &swapChain))
      throw std::runtime_error("Unable to create swapchain");

   currentResolution = resolution;
   currentSurfaceFormat = surfaceFormat;

   uint32_t imagesInSwapChain = 0;
   if (VK_SUCCESS != vkGetSwapchainImagesKHR(mainDevice.logicalDevice, swapChain, &imagesInSwapChain, nullptr))
      throw std::runtime_error("Could not get numer of images from swapchain");

   std::vector<VkImage> images;
   images.resize(imagesInSwapChain);

   if (VK_SUCCESS != vkGetSwapchainImagesKHR(mainDevice.logicalDevice, swapChain, &imagesInSwapChain, images.data()))
      throw std::runtime_error("Could not get images from swapchain");

   for (auto& vkImage : images)
   {
      SwapchainImage image = {};
      image.image = vkImage;
      image.imageView = createImageView(mainDevice.logicalDevice, vkImage, currentSurfaceFormat.format, VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT);
      swapChainImages.emplace_back(std::move(image));
   }
}

void VulkanRenderer::checkInstanceExtensionSupported(const char* const* extensionNames, size_t extensionCount) const
{
   uint32_t availableExtensionCount = 0;

   if (VK_SUCCESS != vkEnumerateInstanceExtensionProperties(nullptr, &availableExtensionCount, nullptr))
      throw std::runtime_error("Failed to enumerate extension properties");

   std::vector<VkExtensionProperties> extensions;
   extensions.resize(availableExtensionCount);

   if (VK_SUCCESS != vkEnumerateInstanceExtensionProperties(nullptr, &availableExtensionCount, extensions.data()))
      throw std::runtime_error("Failed to get extension properties");

   for (size_t i = 0; i < extensionCount; ++i)
   {
      bool extensionFound = false;
      for (auto& availableExtension : extensions)
      {
         if (strcmp(extensionNames[i], availableExtension.extensionName) == 0)
         {
            extensionFound = true;
            break;
         }
      }

      if (!extensionFound)
      {
         char exceptionString[1024] = {};
         sprintf(exceptionString, "Missing extension : %s\n", extensionNames[i]);
         throw std::runtime_error(exceptionString);
      }
   }
}

void VulkanRenderer::getPhysicalDevice()
{
   uint32_t deviceCount = 0;
   if (VK_SUCCESS != vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr))
      throw std::runtime_error("Failed to get number of physical devices");

   std::vector<VkPhysicalDevice> devices;
   devices.resize(deviceCount);

   if (VK_SUCCESS != vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data()))
      throw std::runtime_error("Failed to get list of physical devices");

   std::map<VkPhysicalDevice, DeviceScore> devicesWithScore;
   for (auto& device : devices)
   {
      devicesWithScore[device] = checkDeviceSutable(device);
   }

   VkPhysicalDevice selectedDevice = VK_NULL_HANDLE;
   DeviceScore score;
   for (auto& device : devicesWithScore)
   {
      if (device.second.deviceScore > score.deviceScore && device.second.deviceScore > 0)
      {
         score = device.second;
         selectedDevice = device.first;
      }
   }

   if (VK_NULL_HANDLE == selectedDevice)
      throw std::runtime_error("Unable to find a sutable physical device");

   mainDevice.minStorageBufferOffsetAlignment = score.minStorageBufferOffsetAlignment;
   mainDevice.physicalDevice = selectedDevice;
}

DeviceScore VulkanRenderer::checkDeviceSutable(VkPhysicalDevice device) const
{
   DeviceScore score;

   VkPhysicalDeviceProperties properties = {};
   vkGetPhysicalDeviceProperties(device, &properties);

   score.minStorageBufferOffsetAlignment = properties.limits.minStorageBufferOffsetAlignment;

   if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
      ++score.deviceScore;

   VkPhysicalDeviceFeatures features = {};
   vkGetPhysicalDeviceFeatures(device, &features);
   //TODO : Select ony devices with proper features

   QueueFamilyIndices queues = getQueueFamilyIndices(device);

   if (!queues.valid() || !checkDeviceSwapChainSupport(device))
      return {};

   SwapchainDetails swapchainDetails = getSwapchainDetails(device, surface);

   if (swapchainDetails.supportedFormats.empty() || swapchainDetails.supportedPresentationModes.empty())
      return {};

   return score;
}

bool QueueFamilyIndices::valid()
{
   return graphicFamily >= 0 && presentationFamily >= 0;
}

static Mesh loadMesh(VkPhysicalDevice phisicalDevice, VkDevice logicalDevice, VkQueue transferQueue, VkCommandPool transferCommandPool,
   aiMesh* mesh, const aiScene& scene, const std::vector<uint32_t>& materialToTexture)
{
   std::vector<Vertex> vertices(mesh->mNumVertices);
   std::vector<uint16_t> indices;

   aiVector3D* textureCoordonateChannel = nullptr;
   if (mesh->HasTextureCoords(0))
      textureCoordonateChannel = mesh->mTextureCoords[0];

   aiColor4D* vertexColorChannel = nullptr;
   if (mesh->HasVertexColors(0))
      vertexColorChannel = mesh->mColors[0];

   for (size_t i = 0; i < mesh->mNumVertices; ++i)
   {
      Vertex out;
      aiVector3D vertex = mesh->mVertices[i];
      out.position = { mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z };

      if (textureCoordonateChannel)
         out.uv = { textureCoordonateChannel[i].x, textureCoordonateChannel[i].y };
      if (vertexColorChannel)
         out.color = { vertexColorChannel[i].r, vertexColorChannel[i].g,vertexColorChannel[i].b };
      else
         out.color = { 1.0f, 1.0f, 1.0f };

      vertices[i] = std::move(out);
   }

   for (size_t i = 0; i < mesh->mNumFaces; ++i)
   {
      aiFace* face = mesh->mFaces + i;
      for (size_t j = 0; j < face->mNumIndices; ++j)
      {
         indices.push_back(face->mIndices[j]);
      }
   }

   return Mesh(phisicalDevice, logicalDevice, transferQueue, transferCommandPool, vertices, indices, materialToTexture[mesh->mMaterialIndex]);
   
}

static std::vector<Mesh> loadNode(VkPhysicalDevice phisicalDevice, VkDevice logicalDevice, VkQueue transferQueue, VkCommandPool transferCommandPool,
   aiNode* node, const aiScene& scene, const std::vector<uint32_t>& materialToTexture)
{
   std::vector<Mesh> out;
   for (unsigned int i = 0; i < node->mNumMeshes; ++i)
   {
      out.emplace_back(loadMesh(phisicalDevice, logicalDevice, transferQueue, transferCommandPool, scene.mMeshes[node->mMeshes[i]], scene, materialToTexture));
   }

   for (unsigned int i = 0; i < node->mNumChildren; ++i)
   {
      std::vector<Mesh> childMeshes = loadNode(phisicalDevice, logicalDevice, transferQueue, transferCommandPool, node->mChildren[i], scene, materialToTexture);
      for (auto& m : childMeshes)
      {
         out.emplace_back(std::move(m));
      }
   }

   return std::move(out);
}

uint32_t VulkanRenderer::loadModel(const std::string& fileName)
{
   Assimp::Importer importer;
   const aiScene* scene = importer.ReadFile(fileName, aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_JoinIdenticalVertices);
   if (!scene)
      throw std::runtime_error("Failed to load model :" + fileName);


   std::vector<std::string> textureNames(scene->mNumMaterials);
   for (size_t i = 0; i < scene->mNumMaterials; ++i)
   {
      aiMaterial* material = scene->mMaterials[i];

      std::string texturePath;

      if (material->GetTextureCount(aiTextureType_DIFFUSE))
      {
         aiString path = {};
         if (material->GetTexture(aiTextureType_DIFFUSE, 0, &path) == AI_SUCCESS)
         {
            texturePath = path.C_Str();
         }
      }

      size_t lastBackslash = texturePath.rfind('\\');
      if (lastBackslash != std::string::npos)
      {
         textureNames[i] = texturePath.substr(lastBackslash + 1);
      }
      else
      {
         textureNames[i] = std::move(texturePath);
      }
   }

   std::vector<uint32_t> mapMaterialToLoadedTexture(textureNames.size());

   size_t index = 0;
   for (const auto& i : textureNames)
   {
      if (i.empty())
         mapMaterialToLoadedTexture[index++] = 0; // use the empty texture
      else
         mapMaterialToLoadedTexture[index++] = loadTexture(i.c_str());
   }

   std::vector<Mesh> modelMeshes = loadNode(mainDevice.physicalDevice, mainDevice.logicalDevice, graphicsQueue, graphicsCommandPool, scene->mRootNode, *scene, mapMaterialToLoadedTexture);
   meshes.emplace_back(std::move(modelMeshes));

   return static_cast<uint32_t>(meshes.size() - 1);
}

void VulkanRenderer::updateRenderCommands()
{
   for (size_t i = 0; i < swapChainImages.size(); ++i)
   {
      recordCommandBuffers(i);
   }
}

void ImageBuffer::clean(VkDevice logicalDevice)
{
   if (deviceMemory != VK_NULL_HANDLE)
      vkFreeMemory(logicalDevice, deviceMemory, nullptr);
   deviceMemory = VK_NULL_HANDLE;

   if (imageView != VK_NULL_HANDLE)
      vkDestroyImageView(logicalDevice, imageView, nullptr);
   imageView = VK_NULL_HANDLE;

   if (image != VK_NULL_HANDLE)
      vkDestroyImage(logicalDevice, image, nullptr);
   image = VK_NULL_HANDLE;
}
