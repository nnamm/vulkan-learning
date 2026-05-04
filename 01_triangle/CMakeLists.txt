# 実行ファイル（WinMainを使用）
add_executable(VLTriangle WIN32
  main.cpp
)

# 外部ライブラリのリンク
target_link_libraries(VLTriangle PRIVATE
  Vulkan::Vulkan
  glfw
  glm::glm
)

# マクロ定義（GLMのVulkan向け補正）
target_compile_definitions(VLTriangle PRIVATE
  GLM_FORCE_DEPTH_ZERO_TO_ONE
  GLM_FORCE_RADIANS
)

# ---------------------------------------------------
# 必要なDLLの自動コピー（Windows環境用）
# ---------------------------------------------------
if(WIN32)
  add_custom_command(TARGET VLTriangle POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_RUNTIME_DLLS:VLTriangle>
    $<TARGET_FILE_DIR:VLTriangle>
    COMMAND_EXPAND_LISTS
  )
endif()
