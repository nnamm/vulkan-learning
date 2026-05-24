cmake_minimum_required(VERSION 3.25)
project(VulkanLearning LANGUAGES CXX)

# clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 全ターゲット共通のビルド設定
add_library(project_options INTERFACE)
target_compile_features(project_options INTERFACE cxx_std_20)
target_compile_options(project_options INTERFACE
  $<$<CXX_COMPILER_ID:MSVC>:/utf-8>
)

# 外部パッケージ
find_package(Vulkan REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)

# 使用する共通ライブラリステージ
set(LIB_STAGE "stage1")

# 章ごとのディレクトリ
add_subdirectory(01_triangle)
