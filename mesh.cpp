#include "mesh.h"
#include "utils.h"

#include <stdexcept>


Mesh::Mesh(VkPhysicalDevice physicalDevice, VkDevice logicalDevice, VkQueue transferQueue, VkCommandPool transferCommandPool, const std::vector<Vertex>& data) :
   vertexCount(static_cast<uint32_t>(data.size())),
   physicalDevice(physicalDevice),
   logicalDevice(logicalDevice),
   transferQueue(transferQueue),
   transferCommandPool(transferCommandPool)
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
   VkBuffer stagingBuffer = VK_NULL_HANDLE;
   VkDeviceMemory stagingMemory = VK_NULL_HANDLE;

   creteBuffer(physicalDevice, logicalDevice, sizeof(Vertex) * data.size(),
      VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
      &stagingBuffer, &stagingMemory);

   creteBuffer(physicalDevice, logicalDevice, sizeof(Vertex) * data.size(),
      VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, 
      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
      &vertexBuffer, &deviceMemory);

   void* mappedData = nullptr;
   if (VK_SUCCESS != vkMapMemory(logicalDevice, stagingMemory, 0, sizeof(Vertex) * data.size(), 0, &mappedData))
      throw std::runtime_error("Unable to bind buffer memory");

   memcpy(mappedData, data.data(), data.size() * sizeof(Vertex));

   vkUnmapMemory(logicalDevice, stagingMemory);
   mappedData = nullptr;

   copyBuffer(logicalDevice, transferQueue, transferCommandPool, vertexBuffer, stagingBuffer, sizeof(Vertex) * data.size());

   vkFreeMemory(logicalDevice, stagingMemory, nullptr);
   vkDestroyBuffer(logicalDevice, stagingBuffer, nullptr);
}
