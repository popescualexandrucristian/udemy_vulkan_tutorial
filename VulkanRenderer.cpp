#include "VulkanRenderer.h"
#include "utils.h"

#include <map>
#include <set>

VulkanRenderer::VulkanRenderer()
{
}

int VulkanRenderer::init(GLFWwindow* window)
{
   this->window = window;
   try
   {
      createInstance();
      hookDebugMessager();
      creteSurface();
      getPhysicalDevice();
      queueFamilyIndices = getQueueFamilyIndices(mainDevice.physicalDevice);
      swapchainDetails = getSwapchainDetails(mainDevice.physicalDevice, surface);
      createLogicalDevice();// and logical queues
      createSwapChain(); // and swapchain images
      createRenderPass();
      createGraphicsPipeline();
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
   for (auto& img : swapChainImages)
   {
      if (graphicsPipeline != VK_NULL_HANDLE)
         vkDestroyPipeline(mainDevice.logicalDevice, graphicsPipeline, nullptr);

      graphicsPipeline = VK_NULL_HANDLE;

      if (pipelineLayout != VK_NULL_HANDLE)
         vkDestroyPipelineLayout(mainDevice.logicalDevice, pipelineLayout, nullptr);

      pipelineLayout = VK_NULL_HANDLE;

      if (renderPass != VK_NULL_HANDLE)
         vkDestroyRenderPass(mainDevice.logicalDevice, renderPass, nullptr);

      renderPass = VK_NULL_HANDLE;

      if (img.imageView != VK_NULL_HANDLE)
         vkDestroyImageView(mainDevice.logicalDevice, img.imageView, nullptr);

      img.imageView = VK_NULL_HANDLE;
   }

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

VulkanRenderer::~VulkanRenderer()
{
}

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
   VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
   VkDebugUtilsMessageTypeFlagsEXT messageType,
   const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
   void* pUserData) {

   printf("validation layer: %s\n", pCallbackData->pMessage);

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
   if (!checkInstanceExtensionSupported(createInfo.ppEnabledExtensionNames, createInfo.enabledExtensionCount))
      throw std::runtime_error("Not all the requested extensions are available");

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
   //TODO: Add proper vertex inputs
   VkPipelineVertexInputStateCreateInfo vertexInputCreateInfo = {};
   vertexInputCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
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
   rasterCreateInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
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
   VkPipelineLayoutCreateInfo layoutCreateInfo = {};
   layoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
   //TODO: set layouts and push constants

   if (VK_SUCCESS != vkCreatePipelineLayout(mainDevice.logicalDevice, &layoutCreateInfo, nullptr, &pipelineLayout))
      throw std::runtime_error("Unable to create pipeline layout");

   createInfo.layout = pipelineLayout;

   //render pass only one subpass per pipeline
   createInfo.renderPass = renderPass;
   createInfo.subpass = 0;

   //TODO: Setup depth and stencil
   createInfo.pDepthStencilState = nullptr;


   //base
   createInfo.basePipelineHandle = nullptr; //pointer to a pass, only override values in the new pass

   if (VK_SUCCESS != vkCreateGraphicsPipelines(mainDevice.logicalDevice, nullptr, 1, &createInfo, nullptr, &graphicsPipeline))
      throw std::runtime_error("Failed to create pipeline");

   vkDestroyShaderModule(mainDevice.logicalDevice, vertexShaderModule, nullptr);
   vkDestroyShaderModule(mainDevice.logicalDevice, fragmentShaderModule, nullptr);
}

void VulkanRenderer::createRenderPass()
{
   VkRenderPassCreateInfo createInfo = {};
   createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;

   //color attachment
   VkAttachmentDescription colorAttachment = {};
   colorAttachment.format = currentSurfaceFormat.format;
   colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
   colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
   colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
   colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
   colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
   colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
   colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
   createInfo.attachmentCount = 1;
   createInfo.pAttachments = &colorAttachment;

   //subpass
   VkSubpassDescription subpass = {};
   subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

   VkAttachmentReference colourAttachmentReference = {};
   colourAttachmentReference.attachment = 0; // index in createInfo.pAttachments
   colourAttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
   subpass.colorAttachmentCount = 1;
   subpass.pColorAttachments = &colourAttachmentReference;

   createInfo.subpassCount = 1;
   createInfo.pSubpasses = &subpass;

   //dependencies
   std::vector<VkSubpassDependency> subpassDependencies;
   subpassDependencies.resize(2);


   //VK_IMAGE_LAYOUT_UNDEFINED to VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL >

   //start conversion after VK_SUBPASS_EXTERNAL finised reading and at VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT
   subpassDependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
   subpassDependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
   subpassDependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;

   //end conversion when pass 0 finished all read and write ops before VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
   subpassDependencies[0].dstSubpass = 0; //indx in createInfo.pSubpasses
   subpassDependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
   subpassDependencies[0].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;


   //VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL to VK_IMAGE_LAYOUT_PRESENT_SRC_KHR

   //start conversion after pass 0 finised reading and at VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
   subpassDependencies[1].srcSubpass = 0;
   subpassDependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
   subpassDependencies[1].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;

   //end conversion when VK_SUBPASS_EXTERNAL all read and write ops before VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT
   subpassDependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
   subpassDependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
   subpassDependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;

   createInfo.pDependencies = subpassDependencies.data();
   createInfo.dependencyCount = static_cast<uint32_t>(subpassDependencies.size());

   if (VK_SUCCESS != vkCreateRenderPass(mainDevice.logicalDevice, &createInfo, nullptr, &renderPass))
      throw std::runtime_error("Failed to create render pass");
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

bool VulkanRenderer::checkInstanceExtensionSupported(const char* const* extensionNames, size_t extensionCount) const
{
   uint32_t availableExtensionCount = 0;

   if (VK_SUCCESS != vkEnumerateInstanceExtensionProperties(nullptr, &availableExtensionCount, nullptr))
      throw std::runtime_error("Failed to enumerate extension properties");

   std::vector<VkExtensionProperties> extensions;
   extensions.resize(availableExtensionCount);

   if (VK_SUCCESS != vkEnumerateInstanceExtensionProperties(nullptr, &availableExtensionCount, extensions.data()))
      throw std::runtime_error("Failed to get extension properties");

   size_t foundExtensions = 0;
   for (auto& availableExtension : extensions)
   {
      for (size_t extensionIndex = 0; extensionIndex < extensionCount; ++extensionIndex)
      {
         if (strcmp(extensionNames[extensionIndex], availableExtension.extensionName) == 0)
         {
            ++foundExtensions;
            break;
         }
      }
   }

   return foundExtensions == extensionCount;
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

   std::map<VkPhysicalDevice, uint32_t> devicesWithScore;
   for (auto& device : devices)
   {
      devicesWithScore[device] = checkDeviceSutable(device);
   }

   VkPhysicalDevice selectedDevice = VK_NULL_HANDLE;
   uint32_t score = 0;
   for (auto& device : devicesWithScore)
   {
      if (device.second > score && device.second > 0)
      {
         score = device.second;
         selectedDevice = device.first;
      }
   }

   if (VK_NULL_HANDLE == selectedDevice)
      throw std::runtime_error("Unable to find a sutable physical device");


   this->mainDevice.physicalDevice = selectedDevice;
}

uint32_t VulkanRenderer::checkDeviceSutable(VkPhysicalDevice device) const
{
   uint32_t score = 0;

   VkPhysicalDeviceProperties properties = {};
   vkGetPhysicalDeviceProperties(device, &properties);

   if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
      ++score;

   VkPhysicalDeviceFeatures features = {};
   vkGetPhysicalDeviceFeatures(device, &features);
   //TODO : Select ony devices with proper features

   QueueFamilyIndices queues = getQueueFamilyIndices(device);

   if (!queues.valid() || !checkDeviceSwapChainSupport(device))
      return 0;

   SwapchainDetails swapchainDetails = getSwapchainDetails(device, surface);

   if (swapchainDetails.supportedFormats.empty() || swapchainDetails.supportedPresentationModes.empty())
      return 0;

   return score;
}

bool QueueFamilyIndices::valid()
{
   return graphicFamily >= 0 && presentationFamily >= 0;
}
