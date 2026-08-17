# ---------------------------------------------------
# ヘッダの自己完結性を検査するターゲット
# ---------------------------------------------------
# 通常のビルドでは .h は翻訳単位にならないため、clang-tidy はヘッダを
# main file として解析できない（misc-include-cleaner は main file しか見ない）。
# ここで各ヘッダを単独の翻訳単位としてコンパイルし、compile_commands.json に
# 載せることで、run-clang-tidy が「使う型の include を自分で持っているか」を
# ヘッダに対しても見られるようになる。
#
# 検査対象は自動収集する。明示列挙だと、新しいヘッダを足したときに
# ここへの追記を忘れた分が黙って未検査になるため。
file(GLOB_RECURSE STAGE1_HEADERS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.h")

# .h の拡張子は既定でヘッダ扱い（GCC/Clang ではプリコンパイル済みヘッダの生成）に
# なるため、C++ ソースとして扱うよう明示する。CMake が -x c++ / /TP を付ける。
set_source_files_properties(${STAGE1_HEADERS} PROPERTIES LANGUAGE CXX)

# OBJECT ライブラリ。生成物は使わず、コンパイルが通ること自体が目的。
add_library(stage1_header_check OBJECT ${STAGE1_HEADERS})

# 章のターゲットと同じ条件で検査する（条件がずれると、検査が通っても
# 実際のビルドで壊れる／その逆が起きる）
target_include_directories(stage1_header_check PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(stage1_header_check PRIVATE
  project_options
  Vulkan::Vulkan
  glfw
  glm::glm
)

# ヘッダを main file としてコンパイルする以上、#pragma once は必ず
# 「ヘッダの外にある」と判定される（-Wpragma-once-outside-header）。
# 手法に由来する必然で、コード側に問題があるわけではないため、この検査でのみ抑止する。
target_compile_options(stage1_header_check PRIVATE
  $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wno-pragma-once-outside-header>
)

target_compile_definitions(stage1_header_check PRIVATE
  GLFW_INCLUDE_VULKAN
  GLM_FORCE_DEPTH_ZERO_TO_ONE
  GLM_FORCE_RADIANS
)
