#pragma once
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm.hpp>
#include <gtc/matrix_transform.hpp>
#include <vulkan/vulkan.h>
#include <vector>

struct Vertex
{
   glm::vec3 position = {};
   glm::vec3 color = {};
   glm::vec2 uv = {};
};

struct UboModel
{
   glm::mat4 model = glm::identity<glm::mat4>();
};

struct PushModel
{
   glm::vec3 color = { 1.0f, 1.0f, 1.0f };
};

class Mesh
{
public:
   Mesh() {};
   Mesh(Mesh&& other);
   Mesh(VkPhysicalDevice physicalDevice, VkDevice logicalDevice, VkQueue transferQueue, VkCommandPool transferCommandPool, const std::vector<Vertex>& vertices, const std::vector<uint16_t>& indices, size_t textureId);
   Mesh(const Mesh& other) = default;

   uint32_t getVertexCount() const;
   uint32_t getIndicesCount() const;
   VkBuffer getVertexBuffer() const;
   VkBuffer getIndexBuffer() const;

   const size_t getTextureId() const;

   void clean();

   ~Mesh();

private:
   uint32_t vertexCount = 0;
   uint32_t indicesCount = 0;
   VkBuffer verticesBuffer = VK_NULL_HANDLE;
   VkDeviceMemory verticesMemory = VK_NULL_HANDLE;
   VkBuffer indicesBuffer = VK_NULL_HANDLE;
   VkDeviceMemory indicesMemory = VK_NULL_HANDLE;

   size_t textureId = 0;

   VkDevice logicalDevice = VK_NULL_HANDLE;

   void createVertexBuffer(VkPhysicalDevice physicalDevice, VkDevice logicalDevice, VkQueue transferQueue, VkCommandPool transferCommandPool, const std::vector<Vertex>& vertices, const std::vector<uint16_t>& indices);
};

class MeshModel
{
public:
   MeshModel(std::vector<Mesh>&& meshList);

   uint32_t getMeshCount() const;

   const Mesh* getMesh(uint32_t index) const;

   const glm::mat4& getModel() const;
   void setModel(const glm::mat4&);

   void clean();

   ~MeshModel();
private:
   std::vector<Mesh> meshList;
   glm::mat4 model = glm::identity<glm::mat4>();
};