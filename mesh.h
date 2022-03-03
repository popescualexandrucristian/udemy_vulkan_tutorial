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
   Mesh(VkPhysicalDevice physicalDevice, VkDevice logicalDevice, const std::vector<Vertex>& data);

   uint32_t getVertexCount() const;
   VkBuffer getVertexBuffer() const;

   void clean();

private:
   uint32_t vertexCount = 0;
   VkBuffer vertexBuffer = VK_NULL_HANDLE;
   VkDeviceMemory deviceMemory = VK_NULL_HANDLE;

   VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
   VkDevice logicalDevice = VK_NULL_HANDLE;

   void createVertexBuffer(const std::vector<Vertex>& data);
   uint32_t findMemoryTypeIndex(uint32_t allowedTypes, VkMemoryPropertyFlags properties) const;
};

