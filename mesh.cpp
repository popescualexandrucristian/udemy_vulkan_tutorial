#include "mesh.h"
#include "utils.h"

#include <stdexcept>


Mesh::Mesh(VkPhysicalDevice physicalDevice, VkDevice logicalDevice, VkQueue transferQueue, VkCommandPool transferCommandPool, const std::vector<Vertex>& vertices, const std::vector<uint16_t>& indices) :
   vertexCount(static_cast<uint32_t>(vertices.size())),
   indicesCount(static_cast<uint32_t>(indices.size())),
   physicalDevice(physicalDevice),
   logicalDevice(logicalDevice),
   transferQueue(transferQueue),
   transferCommandPool(transferCommandPool)
{
   createVertexBuffer(vertices, indices);
}

uint32_t Mesh::getVertexCount() const
{
   return vertexCount;
}

uint32_t Mesh::getIndicesCount() const
{
   return indicesCount;
}

VkBuffer Mesh::getVertexBuffer() const
{
   return verticesBuffer;
}

VkBuffer Mesh::getIndexBuffer() const
{
   return indicesBuffer;
}

void Mesh::setUboModel(const UboModel& in)
{
   uboModel = in;
}

const UboModel& Mesh::getUboModel() const
{
   return uboModel;
}

void Mesh::setPushModel(const PushModel& pushModel)
{
   this->pushModel = pushModel;
}

const PushModel& Mesh::getPushModel() const
{
   return pushModel;
}

void Mesh::clean()
{
   if (indicesMemory != VK_NULL_HANDLE)
      vkFreeMemory(logicalDevice, indicesMemory, nullptr);

   indicesMemory = VK_NULL_HANDLE;

   if (indicesBuffer)
      vkDestroyBuffer(logicalDevice, indicesBuffer, nullptr);

   indicesBuffer = VK_NULL_HANDLE;

   if (verticesMemory != VK_NULL_HANDLE)
      vkFreeMemory(logicalDevice, verticesMemory, nullptr);

   verticesMemory = VK_NULL_HANDLE;

   if (verticesBuffer)
      vkDestroyBuffer(logicalDevice, verticesBuffer, nullptr);

   verticesBuffer = VK_NULL_HANDLE;
   vertexCount = 0;
}

void Mesh::createVertexBuffer(const std::vector<Vertex>& vertices, const std::vector<uint16_t>& indices)
{
   VkBuffer stagingBuffer = VK_NULL_HANDLE;
   VkDeviceMemory stagingMemory = VK_NULL_HANDLE;

   creteBuffer(physicalDevice, logicalDevice, 
      std::max(sizeof(Vertex) * vertices.size(), sizeof(uint16_t) * indices.size()),
      VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
      &stagingBuffer, &stagingMemory);

   creteBuffer(physicalDevice, logicalDevice, sizeof(Vertex) * vertices.size(),
      VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, 
      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
      &verticesBuffer, &verticesMemory);

   creteBuffer(physicalDevice, logicalDevice, sizeof(uint16_t) * indices.size(),
      VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
      &indicesBuffer, &indicesMemory);

   void* mappedData = nullptr;
   if (VK_SUCCESS != vkMapMemory(logicalDevice, stagingMemory, 0, sizeof(Vertex) * vertices.size(), 0, &mappedData))
      throw std::runtime_error("Unable to bind buffer memory");

   memcpy(mappedData, vertices.data(), vertices.size() * sizeof(Vertex));

   vkUnmapMemory(logicalDevice, stagingMemory);
   mappedData = nullptr;

   copyBuffer(logicalDevice, transferQueue, transferCommandPool, verticesBuffer, stagingBuffer, sizeof(Vertex) * vertices.size());

   if (VK_SUCCESS != vkMapMemory(logicalDevice, stagingMemory, 0, sizeof(uint16_t) * indices.size(), 0, &mappedData))
      throw std::runtime_error("Unable to bind buffer memory");

   memcpy(mappedData, indices.data(), indices.size() * sizeof(uint16_t));

   vkUnmapMemory(logicalDevice, stagingMemory);
   mappedData = nullptr;

   copyBuffer(logicalDevice, transferQueue, transferCommandPool, indicesBuffer, stagingBuffer, sizeof(uint16_t) * indices.size());

   vkFreeMemory(logicalDevice, stagingMemory, nullptr);
   vkDestroyBuffer(logicalDevice, stagingBuffer, nullptr);
}
