# 実行ファイル（WinMainを使用）
add_executable(VkTriangle WIN32)

# 実行ファイルを作成するためのソースファイル
target_sources(VkTriangle PRIVATE
  main.cpp
  triangle_app.cpp

  "${CMAKE_SOURCE_DIR}/lib/${LIB_STAGE}/core/asset_path.cpp"
  "${CMAKE_SOURCE_DIR}/lib/${LIB_STAGE}/core/buffer_resource.cpp"
  "${CMAKE_SOURCE_DIR}/lib/${LIB_STAGE}/core/command_buffer.cpp"
  "${CMAKE_SOURCE_DIR}/lib/${LIB_STAGE}/core/glfw_surface_provider.cpp"
  "${CMAKE_SOURCE_DIR}/lib/${LIB_STAGE}/core/graphics_pipeline_builder.cpp"
  "${CMAKE_SOURCE_DIR}/lib/${LIB_STAGE}/core/image_barrier.cpp"
  "${CMAKE_SOURCE_DIR}/lib/${LIB_STAGE}/core/shader_loader.cpp"
  "${CMAKE_SOURCE_DIR}/lib/${LIB_STAGE}/core/swapchain.cpp"
  "${CMAKE_SOURCE_DIR}/lib/${LIB_STAGE}/core/vulkan_context.cpp"
)

# 共通ライブラリ
target_include_directories(VkTriangle PRIVATE
  "${CMAKE_SOURCE_DIR}/lib/${LIB_STAGE}"
)

# 外部ライブラリのリンク
target_link_libraries(VkTriangle PRIVATE
  project_options
  Vulkan::Vulkan
  glfw
  glm::glm
)

# マクロ定義（GLMのVulkan向け補正）
target_compile_definitions(VkTriangle PRIVATE
  GLM_FORCE_DEPTH_ZERO_TO_ONE
  GLM_FORCE_RADIANS
)

# ---------------------------------------------------
# 必要なDLLの自動コピー（Windows環境用）
# ---------------------------------------------------
if(WIN32)
  add_custom_command(TARGET VkTriangle POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_RUNTIME_DLLS:VkTriangle>
    $<TARGET_FILE_DIR:VkTriangle>
    COMMAND_EXPAND_LISTS
  )
endif()
