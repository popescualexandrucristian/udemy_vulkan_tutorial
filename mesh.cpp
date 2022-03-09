#include "mesh.h"
#include "utils.h"

#include <stdexcept>


Mesh::Mesh(VkPhysicalDevice physicalDevice, VkDevice logicalDevice, VkQueue transferQueue, VkCommandPool transferCommandPool, const std::vector<Vertex>& vertices, const std::vector<uint16_t>& indices, size_t textureId) :
   vertexCount(static_cast<uint32_t>(vertices.size())),
   indicesCount(static_cast<uint32_t>(indices.size())),
   logicalDevice(logicalDevice),
   textureId(textureId)
{
   createVertexBuffer(physicalDevice, logicalDevice, transferQueue, transferCommandPool, vertices, indices);
}

Mesh::Mesh(Mesh&& other) :
vertexCount(other.vertexCount),
indicesCount(other.indicesCount),
verticesBuffer(other.verticesBuffer),
verticesMemory(other.verticesMemory),
indicesBuffer(other.indicesBuffer),
indicesMemory(other.indicesMemory),
logicalDevice(other.logicalDevice),
textureId(other.textureId)
{
   other.vertexCount = 0;
   other.indicesCount = 0;
   other.verticesBuffer = VK_NULL_HANDLE;
   other.verticesMemory = VK_NULL_HANDLE;
   other.indicesBuffer = VK_NULL_HANDLE;
   other.indicesMemory = VK_NULL_HANDLE;
   other.textureId = 0;
   other.logicalDevice = VK_NULL_HANDLE;
}

Mesh::~Mesh()
{
   clean();
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

const size_t Mesh::getTextureId() const
{
    return textureId;
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

void Mesh::createVertexBuffer(VkPhysicalDevice physicalDevice, VkDevice logicalDevice, VkQueue transferQueue, VkCommandPool transferCommandPool, const std::vector<Vertex>& vertices, const std::vector<uint16_t>& indices)
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

MeshModel::MeshModel(std::vector<Mesh>&& meshList) :
meshList(std::move(meshList))
{
}

MeshModel::~MeshModel()
{
   clean();
}

void MeshModel::setModel(const glm::mat4& model)
{
   this->model = model;
}

uint32_t MeshModel::getMeshCount() const
{
   return static_cast<uint32_t>(meshList.size());
}

const Mesh* MeshModel::getMesh(uint32_t index) const
{
   if (index >= meshList.size())
      return nullptr;

   return &meshList[index];
}

void MeshModel::clean()
{
   for (auto& m : meshList)
      m.clean();

   meshList.clear();
}

const glm::mat4& MeshModel::getModel() const
{
   return model;
}
