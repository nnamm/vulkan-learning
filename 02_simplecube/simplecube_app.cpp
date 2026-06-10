#include "simplecube_app.h"

#include "core/vulkan_context.h"

void SimpleCubeApp::OnInitialize() {
  m_resourceUploader.Initialize();

  CreateDepthBuffer();
  CreateCubeGeometory();
  CreateDescriptorSetLayout();

  CreateUniformBuffers();
  CreateDescriptorSets();

  CreateGraphicsPipeline();
}

void SimpleCubeApp::CreateDepthBuffer() {
  auto& vulkanCtx = VulkanContext::Get();
  auto& swapchain = vulkanCtx.GetSwapchain();
  auto extent = swapchain->GetSwapchain();
  m_depthBuffer = DepthBuffer::Create(extent, VK_FORMAT_D32_SFLOAT);
}
