#pragma once
#include <vector>
#include <string>
#include <vulkan/vulkan.h>

std::vector<char> readFile(const char* filePath);

struct Image
{
   uint32_t width;
   uint32_t height;
   uint32_t number_of_components;
   std::vector<char> data;
   std::string fileName;
};

enum class ReadImageChannels{
   grey = 1,
   grey_alpha = 2,
   rgb = 3,
   rgb_alpha = 4
};

Image readImage(const char* filePath, ReadImageChannels channels);


uint32_t findMemoryTypeIndex(VkPhysicalDevice physicalDevice, uint32_t allowedTypes, VkMemoryPropertyFlags properties);

void creteBuffer(VkPhysicalDevice physicalDevice, VkDevice logicalDevice, VkDeviceSize bufferSize, VkBufferUsageFlags bufferUsage,
   VkMemoryPropertyFlags bufferProperties, VkBuffer* buffer, VkDeviceMemory* bufferMemory);

void copyBuffer(VkDevice logicalDevice, VkQueue transferQueue, VkCommandPool transferCommandPool, VkBuffer destinationBuffer, VkBuffer sourceBuffer, VkDeviceSize size);

void copyImage(VkDevice logicalDevice, VkQueue transferQueue, VkCommandPool transferCommandPool, VkImage destinationImage, VkBuffer sourceBuffer, uint32_t width, uint32_t height);

void transitionImageLayout(VkDevice logicalDevice, VkQueue transferQueue, VkCommandPool transferCommandPool, VkImage image, VkImageLayout currentLayout, VkImageLayout newLayout);