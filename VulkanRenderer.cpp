#include "VulkanRenderer.h"
#include <map>

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
      getPhysicalDevice();
      queueFamilyIndices = getQueueFamilyIndices(mainDevice.physicalDevice);
      createLogicalDevice();
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
   if (VK_NULL_HANDLE != mainDevice.logicalDevice)
      vkDestroyDevice(mainDevice.logicalDevice, nullptr);
#ifdef ENABLE_VALIDATION
   auto vkDestroyDebugUtilsMessengerEXTFunc = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
   if (vkDestroyDebugUtilsMessengerEXTFunc && VK_NULL_HANDLE != debugMessenger)
      vkDestroyDebugUtilsMessengerEXTFunc(instance, debugMessenger, nullptr);
#endif
   if (VK_NULL_HANDLE != instance)
      vkDestroyInstance(instance, nullptr);
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
   VkDeviceQueueCreateInfo queueCreateInfo = {};
   queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
   queueCreateInfo.queueFamilyIndex = queueFamilyIndices.graphicFamily;
   float priority = 1.0f;
   queueCreateInfo.pQueuePriorities = &priority;
   queueCreateInfo.queueCount = 1;

   VkDeviceCreateInfo createInfo = {};
   createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
   createInfo.pQueueCreateInfos = &queueCreateInfo;
   createInfo.queueCreateInfoCount = 1;

   createInfo.enabledExtensionCount = 0;
   createInfo.ppEnabledExtensionNames = nullptr;
   //TODO : Add extensions

   VkPhysicalDeviceFeatures deviceFeatures = {};

   createInfo.pEnabledFeatures = &deviceFeatures;
   //TODO : Add device features

   VkDevice device = VK_NULL_HANDLE;
   if (VK_SUCCESS != vkCreateDevice(mainDevice.physicalDevice, &createInfo, nullptr, &device))
      throw std::runtime_error("Could not create logical device");

   mainDevice.logicalDevice = device;

   vkGetDeviceQueue(mainDevice.logicalDevice, queueFamilyIndices.graphicFamily, 0, &graphicsQueue);
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
      ++index;

      if (out.valid())
      {
         break;
      }
   }

   return out;
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
      if (device.second > score)
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

   if (!queues.valid())
      return 0;

   return score;
}

bool QueueFamilyIndices::valid()
{
   return graphicFamily >= 0;
}
