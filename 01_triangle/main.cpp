#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <filesystem>

#include "core/asset_path.h"
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "core/glfw_surface_provider.h"
#include "core/vulkan_context.h"
#include "triangle_app.h"

namespace fs = std::filesystem;

int __stdcall wWinMain(_In_ HINSTANCE hInstance,
                       _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine,
                       _In_ int nShowCmd) {
  UNREFERENCED_PARAMETER(hPrevInstance);
  UNREFERENCED_PARAMETER(lpCmdLine);

#if _DEBUG
  // エラーをコンソール出力（書籍と違う部分）
  AllocConsole();
  freopen_s(reinterpret_cast<FILE**>(stdout), "CONOUT$", "w", stdout);
  freopen_s(reinterpret_cast<FILE**>(stderr), "CONOUT$", "w", stderr);
#endif

  // カレントディレクトリの設定（本書と異なり、WIN32としてここに統一）
  wchar_t exePath[MAX_PATH];
  GetModuleFileNameW(nullptr, exePath, MAX_PATH);
  fs::path exeDir = fs::path(exePath).parent_path();
  SetCurrentDirectoryW(exeDir.c_str());

  fs::path assetDir = exeDir / "../assets";
  SetAssetRootPath(assetDir);

  // 初期化処理
  glfwInit();
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
}  // namespace std::filesystem
