#pragma once
#include <glm.hpp>
#include <vulkan/vulkan.h>
#include <vector>

struct Vertex
{
   glm::vec3 position;
   glm::vec3 color;
};

class Mesh
{
public:
   Mesh() {};
   Mesh(VkPhysicalDevice physicalDevice, VkDevice logicalDevice, VkQueue transferQueue, VkCommandPool transferCommandPool, const std::vector<Vertex>& vertices, const std::vector<uint16_t>& indices);

   uint32_t getVertexCount() const;
   uint32_t getIndicesCount() const;
   VkBuffer getVertexBuffer() const;
   VkBuffer getIndexBuffer() const;

   void clean();

private:
   uint32_t vertexCount = 0;
   uint32_t indicesCount = 0;
   VkBuffer verticesBuffer = VK_NULL_HANDLE;
   VkDeviceMemory verticesMemory = VK_NULL_HANDLE;
   VkBuffer indicesBuffer = VK_NULL_HANDLE;
   VkDeviceMemory indicesMemory = VK_NULL_HANDLE;

   VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
   VkDevice logicalDevice = VK_NULL_HANDLE;

   VkQueue transferQueue = VK_NULL_HANDLE;
   VkCommandPool transferCommandPool = VK_NULL_HANDLE;

   void createVertexBuffer(const std::vector<Vertex>& vertices, const std::vector<uint16_t>& indices);
};

