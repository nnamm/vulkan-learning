#include "simplecube_app.h"

#include <cstdint>

#include "core/buffer_resource.h"
#include "core/vulkan_context.h"

void SimpleCubeApp::OnInitialize() {
    m_resourceUploader.Initialize();

    CreateDepthBuffer();
    CreateCubeGeometry();
    CreateDescriptorSetLayout();

    CreateUniformBuffers();
    CreateDescriptorSets();

    CreateGraphicsPipeline();
}

void SimpleCubeApp::CreateCubeGeometry() {
    const glm::vec3 A(-0.5f, 0.5f, 0.5f), B(-0.5f, -0.5f, 0.5f), C(0.5f, 0.5f, 0.5f),
        D(0.5f, -0.5f, 0.5f), E(-0.5f, 0.5f, -0.5f), F(-0.5f, -0.5f, -0.5f), G(0.5f, 0.5f, -0.5f),
        H(0.5f, -0.5f, -0.5f);

    std::vector<Vertex> vertices = {
        // front
        {A, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 1.0f}},
        {B, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}},
        {C, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 1.0f}},
        {D, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 0.0f}},
        // back
        {E, {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f, 1.0f}},
        {F, {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f, 0.0f}},
        {G, {0.0f, 0.0f, -1.0f}, {0.0f, 1.0f, 1.0f}},
        {H, {0.0f, 0.0f, -1.0f}, {0.0f, 1.0f, 0.0f}},
        // right
        {C, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}},
        {D, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 0.0f}},
        {G, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 1.0f}},
        {H, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
        // left
        {E, {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
        {F, {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
        {A, {-1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 1.0f}},
        {B, {-1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}},
        // top
        {E, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
        {A, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 1.0f}},
        {G, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 1.0f}},
        {C, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 1.0f}},
        // bottom
        {B, {0.0f, -1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}},
        {F, {0.0f, -1.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
        {D, {0.0f, -1.0f, 0.0f}, {1.0f, 1.0f, 0.0f}},
        {H, {0.0f, -1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
    };

    std::vector<uint32_t> indices = {
        0,  1,  2,  2,  1,  3,   // front
        6,  7,  4,  4,  7,  5,   // back
        8,  9,  10, 10, 9,  11,  // right
        12, 13, 14, 14, 13, 15,  // left
        16, 17, 18, 18, 17, 19,  // top
        20, 21, 22, 22, 21, 23,  // bottom
    };

    VkDeviceSize bufferSize;
    VkMemoryPropertyFlags memProps = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    bufferSize = sizeof(Vertex) * vertices.size();
    m_cube.vertexBuffer = VertexBuffer::Create(bufferSize, memProps);

    m_resourceUploader.UploadBuffer(m_cube.vertexBuffer.get(), vertices.data(), bufferSize,
                                    VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT);
    bufferSize = sizeof(uint32_t) * indices.size();
    m_cube.indexBuffer = IndexBuffer::Create(bufferSize, memProps);
    m_resourceUploader.UploadBuffer(m_cube.indexBuffer.get(), indices.data(), bufferSize,
                                    VK_ACCESS_INDEX_READ_BIT);
    m_cube.indexCount = indices.size();

    m_resourceUploader.SubmitAndWait();
}

void SimpleCubeApp::CreateUniformBuffers() {
    auto& vulkanCtx = VulkanContext::Get();
    assert(vulkanCtx.MaxInflightFrames == m_uniformBuffers.size());

    for (uint32_t i = 0; i < m_uniformBuffers.size(); ++i) {
        m_uniformBuffers[i] = UniformBuffer::Create(sizeof(SceneConstants));
    }
}

void SimpleCubeApp::CreateDepthBuffer() {
    auto& vulkanCtx = VulkanContext::Get();
    auto& swapchain = vulkanCtx.GetSwapchain();
    auto extent = swapchain->GetExtent();
    m_depthBuffer = DepthBuffer::Create(extent, VK_FORMAT_D32_SFLOAT);
}
