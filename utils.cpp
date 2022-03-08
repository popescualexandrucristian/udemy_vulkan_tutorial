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

Image readImage(const char* filePath, ReadImageChannels channels)
{
   Image out;

   FILE* file = fopen(filePath, "rb");
   if (!file)
      return out;

   int width = 0;
   int height = 0;
   int requestedComponents = static_cast<int>(channels);
   int outputedComponents = 0;
   stbi_uc* status = stbi_load_from_file(file, &width, &height, &outputedComponents, requestedComponents);

   fclose(file);

   if (!status)
      return out;

   out.data.resize(width * height * requestedComponents);

   memcpy(out.data.data(), status, out.data.size());
   out.width = static_cast<uint32_t>(width);
   out.height = static_cast<uint32_t>(height);
   out.number_of_components = outputedComponents;
   out.fileName = filePath;
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

static VkCommandBuffer beginCopyCommandBuffer(VkDevice logicalDevice, VkCommandPool commandPool)
{
   VkCommandBuffer transferCommandBuffer = VK_NULL_HANDLE;

   VkCommandBufferAllocateInfo transferBufferAllocationInfo = {};
   transferBufferAllocationInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
   transferBufferAllocationInfo.commandBufferCount = 1;
   transferBufferAllocationInfo.commandPool = commandPool;
   transferBufferAllocationInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

   if (VK_SUCCESS != vkAllocateCommandBuffers(logicalDevice, &transferBufferAllocationInfo, &transferCommandBuffer))
      throw std::runtime_error("Unable to allocate the transfer command buffer");

   VkCommandBufferBeginInfo transferBeginInfo = {};
   transferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
   transferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

   if (VK_SUCCESS != vkBeginCommandBuffer(transferCommandBuffer, &transferBeginInfo))
      throw std::runtime_error("Unable to begin the transfer command buffer");

   return transferCommandBuffer;
}

static void endCopyCommandBuffer(VkDevice logicalDevice, VkQueue queue, VkCommandPool commandPool, VkCommandBuffer commandBuffer)
{
   VkFence transferFence = VK_NULL_HANDLE;
   VkFenceCreateInfo fenceCreateInfo = {};
   fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

   if (VK_SUCCESS != vkCreateFence(logicalDevice, &fenceCreateInfo, nullptr, &transferFence))
      throw std::runtime_error("Unable to create transfer fence");

   vkEndCommandBuffer(commandBuffer);

   VkSubmitInfo submitInfo = {};
   submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
   submitInfo.pCommandBuffers = &commandBuffer;
   submitInfo.commandBufferCount = 1;

   if (VK_SUCCESS != vkQueueSubmit(queue, 1, &submitInfo, transferFence))
      throw std::runtime_error("Unable to submit transfer buffer");

   if (VK_SUCCESS != vkWaitForFences(logicalDevice, 1, &transferFence, VK_FALSE, UINT64_MAX))
      throw std::runtime_error("Error while waiting for transfer to complete");

   vkDestroyFence(logicalDevice, transferFence, nullptr);
   vkFreeCommandBuffers(logicalDevice, commandPool, 1, &commandBuffer);
}

void copyBuffer(VkDevice logicalDevice, VkQueue transferQueue, VkCommandPool transferCommandPool, VkBuffer destinationBuffer, VkBuffer sourceBuffer, VkDeviceSize size)
{
   VkCommandBuffer transferCommandBuffer = beginCopyCommandBuffer(logicalDevice, transferCommandPool);

   VkBufferCopy region = {};
   region.size = size;
   vkCmdCopyBuffer(transferCommandBuffer, sourceBuffer, destinationBuffer, 1, &region);

   endCopyCommandBuffer(logicalDevice, transferQueue, transferCommandPool, transferCommandBuffer);
}

void transitionImageLayout(VkDevice logicalDevice, VkQueue transferQueue, VkCommandPool transferCommandPool, VkImage image, VkImageLayout currentLayout, VkImageLayout newLayout)
{
   VkCommandBuffer commandBuffer = beginCopyCommandBuffer(logicalDevice, transferCommandPool);

   VkImageMemoryBarrier memoryBarier = {};
   memoryBarier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
   memoryBarier.oldLayout = currentLayout;
   memoryBarier.newLayout = newLayout;
   memoryBarier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
   memoryBarier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
   memoryBarier.image = image;
   memoryBarier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
   memoryBarier.subresourceRange.baseArrayLayer = 0;
   memoryBarier.subresourceRange.baseMipLevel = 0;
   memoryBarier.subresourceRange.layerCount = 1;
   memoryBarier.subresourceRange.levelCount = 1;

   VkPipelineStageFlags sourceStage = VK_PIPELINE_STAGE_NONE;
   VkPipelineStageFlags destinationStage = VK_PIPELINE_STAGE_NONE;

   if (currentLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
   {
      memoryBarier.srcAccessMask = VK_ACCESS_NONE;
      memoryBarier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

      sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
      destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
   } 
   else if (currentLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
   {
      memoryBarier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
      memoryBarier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

      sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
      destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
   }
   else
   {
      throw std::runtime_error("Unknown transition");
   }

   vkCmdPipelineBarrier(commandBuffer,
      sourceStage, destinationStage,
      0,
      0, nullptr,
      0, nullptr,
      1, &memoryBarier);

   endCopyCommandBuffer(logicalDevice, transferQueue, transferCommandPool, commandBuffer);
}

void copyImage(VkDevice logicalDevice, VkQueue transferQueue, VkCommandPool transferCommandPool, VkImage destinationImage, VkBuffer sourceBuffer, uint32_t width, uint32_t height)
{
   VkCommandBuffer transferCommandBuffer = beginCopyCommandBuffer(logicalDevice, transferCommandPool);

   VkBufferImageCopy region = {};
   region.bufferOffset = 0;
   region.bufferRowLength = 0;
   region.bufferImageHeight = 0;
   region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
   region.imageSubresource.mipLevel = 0;
   region.imageSubresource.baseArrayLayer = 0;
   region.imageSubresource.layerCount = 1;
   region.imageOffset = { 0, 0, 0 };
   region.imageExtent = { width, height, 1 };

   vkCmdCopyBufferToImage(transferCommandBuffer, sourceBuffer, destinationImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

   endCopyCommandBuffer(logicalDevice, transferQueue, transferCommandPool, transferCommandBuffer);
}