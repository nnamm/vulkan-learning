#pragma once
#include <vulkan/vulkan.h>

#include <filesystem>

namespace loader {
VkShaderModule LoadShaderModule(const std::filesystem::path& shaderSpvPath);
};
