#include "simplecube_app.h"

void SimpleCubeApp::OnInitialize() {
  m_resourceUploader.Initialize();

  CreateDepthBuffer();
  CreateCubeGeometory();
  CreateDescriptorSetLayout();

  CreateUniformBuffers();
  CreateDescriptorSets();

  CreateGraphicsPipeline();
}
