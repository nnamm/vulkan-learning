#include "core/resource_uploader.h"

#include <cstdint>
#include <cstring>

#include "core/buffer_resource.h"
#include "core/vulkan_context.h"

bool ResourceUploader::UploadBuffer(IBufferResource* target, const void* pData, size_t size,
                                    VkAccessFlags nextAccessMask) {
    VulkanContext& context = VulkanContext::Get();
    VkDevice device = context.GetVkDevice();

    if (target->IsHostAccessible()) {
        // 直接書き込み可能のため、ここで処理する
        if (void* p = target->Map(); p != nullptr) {
            std::memcpy(p, pData, size);
            target->Unmap();
            return true;
        }
        return false;
    }

    auto stagingBuffer = StagingBuffer::Create(size);
    if (!stagingBuffer) {
        return false;
    }

    void* mapped = stagingBuffer->Map();
    std::memcpy(mapped, pData, size);
    stagingBuffer->Unmap();

    m_transferEntries.emplace_back(PendingTransfer{
        .stagingBuffer = std::move(stagingBuffer),
        .destinationBuffer = target,
        .dstAccessMask = nextAccessMask,
    });
    return true;
}

void ResourceUploader::SubmitAndWait() {
    if (m_transferEntries.empty()) {
        return;
    }
    VulkanContext& vulkanCtx = VulkanContext::Get();
    VkDevice device = vulkanCtx.GetVkDevice();
    VkCommandPool pool = vulkanCtx.GetCommandPool();
    VkQueue queue = vulkanCtx.GetGraphicsQueue();

    // コマンドバッファ確保
    auto commandBuffer = vulkanCtx.CreateCommandBuffer();
    commandBuffer->Begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    // 転送処理を先にすべて記録
    for (auto& entry : m_transferEntries) {
        IBufferResource* dst = entry.destinationBuffer;
        auto& src = entry.stagingBuffer;

        VkBufferCopy copyRegion{};
        copyRegion.srcOffset = 0;
        copyRegion.dstOffset = 0;
        copyRegion.size = dst->GetBufferSize();

        vkCmdCopyBuffer(*commandBuffer, src->GetVkBuffer(), dst->GetVkBuffer(), 1, &copyRegion);
    }

    // 転送後、バリアをまとめて1回発行
    std::vector<VkBufferMemoryBarrier2> barriers;
    for (auto& entry : m_transferEntries) {
        IBufferResource* dst = entry.destinationBuffer;
        auto dstAccessMask = dst->GetAccessFlags();

        VkBufferMemoryBarrier2 barrier{
            .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
            .srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
            .dstAccessMask = dstAccessMask,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .buffer = dst->GetVkBuffer(),
            .offset = 0,
            .size = dst->GetBufferSize(),
        };
        barriers.push_back(barrier);
    }

    if (!barriers.empty()) {
        VkDependencyInfo depInfo{
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .bufferMemoryBarrierCount = static_cast<uint32_t>(barriers.size()),
            .pBufferMemoryBarriers = barriers.data(),
        };
        vkCmdPipelineBarrier2(*commandBuffer, &depInfo);
    }

    commandBuffer->End();

    auto cmd = commandBuffer->Get();
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;

    // コマンドバッファを実行し、完了まで待機する
    vkResetFences(device, 1, &m_transferFence);
    vkQueueSubmit(queue, 1, &submitInfo, m_transferFence);
    vkWaitForFences(device, 1, &m_transferFence, VK_TRUE, UINT64_MAX);

    m_transferEntries.clear();
    commandBuffer.reset();
}
