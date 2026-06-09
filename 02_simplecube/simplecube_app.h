#pragma once
#include <array>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "common/sample_app.h"

// GLMで設定する値単位をラジアンに
// #define GLM_FORCE_RADIANS
// Vulkan ではClip空間 Z:[0,1]のため定義が必要
// #define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include "core/asset_path.h"
#include "core/buffer_resource.h"
#include "core/image_resource.h"
#include "core/resource_uploader.h"
#include "core/shader_loader.h"
#include "core/swapchain.h"
#include "core/vulkan_context.h"
#include "glm/ext.hpp"
#include "glm/glm.hpp"

class SimpleCubeApp : public ISampleApp {
 public:
  virtual void OnInitialize() override;
  virtual void OnDrawFrame() override;
  virtual void OnCleanup() override;

  struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec3 color;
  };
  struct SceneConstants {
    glm::mat4 mtxWorld;
    glm::mat4 mtxView;
    glm::mat4 mtxProj;
    glm::mat4 lightDir;
    glm::mat4 eyePosition;
  };

 private:
  void CreateCubeGeometory();
  void CreateSphereGeomeoty();
  void CreateDescriptorSetLayout();
  void CreateUniformBuffers();
  void CreateDescriptorSets();
  void CreateDepthBuffer();
  void CreateGraphicsPipeline();

  ResourceUploader m_resourceUploader{};

  VkPipeline m_pipeline = VK_NULL_HANDLE;
  VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;

  struct {
    std::shared_ptr<VertexBuffer> vertexBuffer;
    std::shared_ptr<IndexBuffer> indexBuffer;

    uint32_t indexCount;
  } m_cube{};
  VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;

  std::array<std::shared_ptr<UniformBuffer>, 2> m_uniformBuffers;
  std::array<VkDescriptorSet, 2> m_descriptorSets;

  std::shared_ptr<DepthBuffer> m_depthBuffer;
};
