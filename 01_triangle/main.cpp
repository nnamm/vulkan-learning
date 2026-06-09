#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstdio>
#include <exception>
#elif defined(__linux__)
#include <linux/limits.h>
#include <unistd.h>

#include <iostream>
#include <stdexcept>
#endif

#include <cstdint>
#include <filesystem>

#include "core/asset_path.h"
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "core/glfw_surface_provider.h"
#include "core/vulkan_context.h"
#include "triangle_app.h"

namespace fs = std::filesystem;

int mainEntryPoint() {
  // 初期化処理
  if (!glfwInit()) {
    return -1;
  };
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

  // ウィンドウの作成
  GLFWwindow* window =
      glfwCreateWindow(1280, 720, "Triangle", nullptr, nullptr);
  GLFWSurfaceProvider surfaceProvider(window);

  // Vulkanの初期化
  auto& vulkanCtx = VulkanContext::Get();
  vulkanCtx.GetWindowSystemExtensions = [=](auto& extensionList) {
    uint32_t extCount = 0;
    const char** extensions = glfwGetRequiredInstanceExtensions(&extCount);
    if (extCount > 0) {
      extensionList.insert(extensionList.end(), extensions,
                           extensions + extCount);
    }
  };
  vulkanCtx.Initialize("Triangle", &surfaceProvider);
  vulkanCtx.RecreateSwapchain();

  // アプリケーションの初期化
  TriangleApp theApp{};
  theApp.OnInitialize();

  // メッセージループ処理
  while (glfwWindowShouldClose(window) == GLFW_FALSE) {
    glfwPollEvents();

    // 描画処理
    theApp.OnDrawFrame();
  }

  // 終了処理
  theApp.OnCleanup();
  vulkanCtx.Cleanup();

  glfwDestroyWindow(window);
  glfwTerminate();
  return 0;
}

#if defined(_WIN32)
static fs::path GetExecutableDir() {
  wchar_t exePath[MAX_PATH];
  GetModuleFileNameW(nullptr, exePath, MAX_PATH);
  return fs::path(exePath).parent_path();
}
#elif defined(__linux__)
static fs::path GetExecutableDir() {
  char exePath[PATH_MAX] = {0};
  ssize_t len = readlink("/proc/self/exe", exePath, sizeof(exePath) - 1);
  if (len == -1) {
    throw std::runtime_error("Failed to read /proc/self/exe");
  }
  exePath[len] = '\0';
  return fs::path(exePath).parent_path();
}
#endif

// OS非依存の共通セットアップ
static int run() {
  fs::path exeDir = GetExecutableDir();
  fs::current_path(exeDir);
  SetAssetRootPath(exeDir / "../../../assets");
  return mainEntryPoint();
}

#if defined(_WIN32)
int __stdcall wWinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPWSTR,
                       _In_ int) {
  try {
#if defined(_DEBUG)
    // エラーをコンソール出力
    AllocConsole();
    FILE* fpOut = nullptr;
    FILE* fpErr = nullptr;
    freopen_s(&fpOut, "CONOUT$", "w", stdout);
    freopen_s(&fpErr, "CONOUT$", "w", stderr);
    SetConsoleOutputCP(CP_UTF8);
#endif
    return run();
  } catch (const std::exception& e) {
    MessageBoxA(nullptr, e.what(), "Exception", MB_OK | MB_ICONERROR);
    return -1;
  } catch (...) {
    MessageBoxA(nullptr, "Unknown exception", "Exception",
                MB_OK | MB_ICONERROR);
    return -1;
  }
}
#elif defined(__linux__)
int main() {
  try {
    return run();
  } catch (const std::exception& e) {
    std::cerr << "Exception: " << e.what() << std::endl;
    return -1;
  }
}
#endif
