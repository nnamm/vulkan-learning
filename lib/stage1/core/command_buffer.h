#pragma once
#include <memory>

#include "core/image_barrier.h"
#include "core/vulkan_context.h"

class CommandBuffer {
  public:
    explicit CommandBuffer(VkCommandBuffer commandBuffer);
    virtual ~CommandBuffer();

    void Begin(VkCommandBufferUsageFlags usageFlag = 0);
    void End();
    void Reset();

    VkCommandBuffer Get() const { return m_commandBuffer; }

    // 書籍サンプルの設計として意図された暗黙変換。vkCmd*(*commandBuffer, ...) のように
    // そのまま Vulkan API へ渡すために使う。C++ の慣用では変換演算子を削除して Get() に
    // 一本化するところだが、写経の型（書籍との1対1の対応）を保つため残している。
    // NOLINTBEGIN(google-explicit-constructor)
    operator VkCommandBuffer() { return m_commandBuffer; }
    operator VkCommandBuffer() const { return m_commandBuffer; }
    // NOLINTEND(google-explicit-constructor)

    void TransitionLayout(VkImage image, const VkImageSubresourceRange& range,
                          const ImageLayoutTransition& transition);

    template <typename T>
    void TransitionLayout(std::shared_ptr<T> image, const ImageLayoutTransition& transition) {
        TransitionLayout(image->GetVkImage(), image->GetSubresourceRange(), transition);
        image->SetAccessFlag(transition.dstAccessMask);
        image->SetLayout(transition.newLayout);
    }

  private:
    VkCommandBuffer m_commandBuffer{};
};
