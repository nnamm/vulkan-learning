cmake_minimum_required(VERSION 3.25)
project(VulkanLearning LANGUAGES CXX)

# clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# C++20をデフォルト
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 外部パッケージ
find_package(Vulkan REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)

# 全ターゲット共通のインクルードパス
# （lib/以下を全サブプロジェクトから参照できるように）
include_directories(${CMAKE_SOURCE_DIR}/lib/stage1)

# 章ごとのディレクトリ
add_subdirectory(01_triangle)
