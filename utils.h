#pragma once
#include <vector>
#include <vulkan/vulkan.h>

std::vector<char> readFile(const char* filePath);

struct Image
{
   uint32_t width;
   uint32_t height;
   uint32_t number_of_components;
   std::vector<char> data;
};

Image readImage(const char* filePath);


uint32_t findMemoryTypeIndex(VkPhysicalDevice physicalDevice, uint32_t allowedTypes, VkMemoryPropertyFlags properties);

void creteBuffer(VkPhysicalDevice physicalDevice, VkDevice logicalDevice, VkDeviceSize bufferSize, VkBufferUsageFlags bufferUsage,
   VkMemoryPropertyFlags bufferProperties, VkBuffer* buffer, VkDeviceMemory* bufferMemory);

void copyBuffer(VkDevice logicalDevice, VkQueue transferQueue, VkCommandPool transferCommandPool, VkBuffer destinationBuffer, VkBuffer sourceBuffer, VkDeviceSize size);