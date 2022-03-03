#include "mesh.h"

#include <stdexcept>


Mesh::Mesh(VkPhysicalDevice physicalDevice, VkDevice logicalDevice, const std::vector<Vertex>& data) :
   vertexCount(static_cast<uint32_t>(data.size())),
   physicalDevice(physicalDevice),
   logicalDevice(logicalDevice)
{
   createVertexBuffer(data);
}

uint32_t Mesh::getVertexCount() const
{
   return vertexCount;
}

VkBuffer Mesh::getVertexBuffer() const
{
   return vertexBuffer;
}

void Mesh::clean()
{
   if (deviceMemory != VK_NULL_HANDLE)
      vkFreeMemory(logicalDevice, deviceMemory, nullptr);

   deviceMemory = VK_NULL_HANDLE;

   if (vertexBuffer)
      vkDestroyBuffer(logicalDevice, vertexBuffer, nullptr);

   vertexBuffer = VK_NULL_HANDLE;
   vertexCount = 0;
}

void Mesh::createVertexBuffer(const std::vector<Vertex>& data)
{
   VkBufferCreateInfo createInfo = {};
   createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
   createInfo.size = sizeof(Vertex) * data.size();
   createInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
   createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

   if (VK_SUCCESS != vkCreateBuffer(logicalDevice, &createInfo, nullptr, &vertexBuffer))
      throw std::runtime_error("Unable to create buffer");

   VkMemoryRequirements memoryRequierments = {};
   vkGetBufferMemoryRequirements(logicalDevice, vertexBuffer, &memoryRequierments);

   VkMemoryAllocateInfo allocInfo = {};
   allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
   allocInfo.allocationSize = memoryRequierments.size;
   allocInfo.memoryTypeIndex = findMemoryTypeIndex(memoryRequierments.memoryTypeBits, 
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

   if (VK_SUCCESS != vkAllocateMemory(logicalDevice, &allocInfo, nullptr, &deviceMemory))
      throw std::runtime_error("Unable to allocate buffer memory");

   if (VK_SUCCESS != vkBindBufferMemory(logicalDevice, vertexBuffer, deviceMemory, 0))
      throw std::runtime_error("Unable to bind buffer memory");

   void* mappedData = nullptr;
   if (VK_SUCCESS != vkMapMemory(logicalDevice, deviceMemory, 0, sizeof(Vertex) * data.size(), 0, &mappedData))
      throw std::runtime_error("Unable to bind buffer memory");

   memcpy(mappedData, data.data(), data.size() * sizeof(Vertex));

   vkUnmapMemory(logicalDevice, deviceMemory);
   mappedData = nullptr;

   //TODO: flush the data ?
}

uint32_t Mesh::findMemoryTypeIndex(uint32_t allowedTypes, VkMemoryPropertyFlags properties) const
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
