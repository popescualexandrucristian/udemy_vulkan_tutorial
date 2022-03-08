#include "utils.h"
#include <iostream>
#include <stdexcept>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

std::vector<char> readFile(const char* filePath)
{
   std::vector<char> out;

   FILE* file = fopen(filePath, "rb");
   if (!file)
      return out;

   fseek(file, 0, SEEK_END);

   long fileSize = ftell(file);

   fseek(file, 0, SEEK_SET);

   out.resize(fileSize);

   long availableData = fileSize;
   while (availableData)
   {
      size_t receivedData = fread(out.data() + (fileSize - availableData), sizeof(char), availableData, file);
      if (ferror(file))
      {
         out.clear();
         break;
      }

      if (feof(file))
         break;

      if (receivedData > 0)
         availableData -= static_cast<long>(receivedData);
   }
   fclose(file);
   return out;
}

Image readImage(const char* filePath)
{
   Image out;

   FILE* file = fopen(filePath, "rb");
   if (!file)
      return out;

   int width = 0;
   int height = 0;
   int comp = 0;
   stbi_uc* status = stbi_load_from_file(file, &width, &height, &comp, comp);

   fclose(file);

   if (!status)
      return out;

   out.data.resize(width * height * comp);

   memcpy(out.data.data(), status, out.data.size());
   out.width = static_cast<uint32_t>(width);
   out.height = static_cast<uint32_t>(height);
   out.number_of_components = comp;

   stbi_image_free(status);
   return out;
}

uint32_t findMemoryTypeIndex(VkPhysicalDevice physicalDevice, uint32_t allowedTypes, VkMemoryPropertyFlags properties)
{
   //TODO: Check available size of heap ? Maybe a manager ?

   VkPhysicalDeviceMemoryProperties physicalDeviceMemoryProperties = {};
   vkGetPhysicalDeviceMemoryProperties(physicalDevice, &physicalDeviceMemoryProperties);
   for (uint32_t i = 0; i < physicalDeviceMemoryProperties.memoryTypeCount; ++i)
   {
      if (allowedTypes & (1 << i))
      {
         if ((physicalDeviceMemoryProperties.memoryTypes[i].propertyFlags & properties) == properties)
            return i;
      }
   }

   return UINT32_MAX;
}

void creteBuffer(VkPhysicalDevice physicalDevice, VkDevice logicalDevice, VkDeviceSize bufferSize, VkBufferUsageFlags bufferUsage,
   VkMemoryPropertyFlags bufferProperties, VkBuffer* buffer, VkDeviceMemory* bufferMemory)
{
   VkBufferCreateInfo bufferCreateInfo = {};
   bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
   bufferCreateInfo.size = bufferSize;
   bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
   bufferCreateInfo.usage = bufferUsage;

   if (VK_SUCCESS != vkCreateBuffer(logicalDevice, &bufferCreateInfo, nullptr, buffer))
      throw std::runtime_error("Unable to create buffer");

   VkMemoryRequirements memoryRequierments = {};
   vkGetBufferMemoryRequirements(logicalDevice, *buffer, &memoryRequierments);

   VkMemoryAllocateInfo allocInfo = {};
   allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
   allocInfo.allocationSize = memoryRequierments.size;
   allocInfo.memoryTypeIndex = findMemoryTypeIndex(physicalDevice, memoryRequierments.memoryTypeBits, bufferProperties);

   if (VK_SUCCESS != vkAllocateMemory(logicalDevice, &allocInfo, nullptr, bufferMemory))
      throw std::runtime_error("Unable to allocate buffer memory");

   if (VK_SUCCESS != vkBindBufferMemory(logicalDevice, *buffer, *bufferMemory, 0))
      throw std::runtime_error("Unable to allocate buffer memory");
}


void copyBuffer(VkDevice logicalDevice, VkQueue transferQueue, VkCommandPool transferCommandPool, VkBuffer destinationBuffer, VkBuffer sourceBuffer, VkDeviceSize size)
{
   VkCommandBuffer transferCommandBuffer = VK_NULL_HANDLE;

   VkCommandBufferAllocateInfo transferBufferAllocationInfo = {};
   transferBufferAllocationInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
   transferBufferAllocationInfo.commandBufferCount = 1;
   transferBufferAllocationInfo.commandPool = transferCommandPool;
   transferBufferAllocationInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

   if (VK_SUCCESS != vkAllocateCommandBuffers(logicalDevice, &transferBufferAllocationInfo, &transferCommandBuffer))
      throw std::runtime_error("Unable to allocate the transfer command buffer");

   VkCommandBufferBeginInfo transferBeginInfo = {};
   transferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
   transferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

   VkFence transferFence = VK_NULL_HANDLE;
   VkFenceCreateInfo fenceCreateInfo = {};
   fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

   if (VK_SUCCESS != vkCreateFence(logicalDevice, &fenceCreateInfo, nullptr, &transferFence))
      throw std::runtime_error("Unable to create transfer fence");

   if (VK_SUCCESS != vkBeginCommandBuffer(transferCommandBuffer, &transferBeginInfo))
      throw std::runtime_error("Unable to begin the transfer command buffer");

   VkBufferCopy region = {};
   region.size = size;
   vkCmdCopyBuffer(transferCommandBuffer, sourceBuffer, destinationBuffer, 1, &region);

   vkEndCommandBuffer(transferCommandBuffer);

   VkSubmitInfo submitInfo = {};
   submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
   submitInfo.pCommandBuffers = &transferCommandBuffer;
   submitInfo.commandBufferCount = 1;

   if (VK_SUCCESS != vkQueueSubmit(transferQueue, 1, &submitInfo, transferFence))
      throw std::runtime_error("Unable to submit transfer buffer");

   if (VK_SUCCESS != vkWaitForFences(logicalDevice, 1, &transferFence, VK_FALSE, UINT64_MAX))
      throw std::runtime_error("Error while waiting for transfer to complete");

   vkDestroyFence(logicalDevice, transferFence, nullptr);
   vkFreeCommandBuffers(logicalDevice, transferCommandPool, 1, &transferCommandBuffer);
}