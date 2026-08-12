# タスク: 推移的インクルードの解消（misc-include-cleaner 30件）

作成: 2026-07-30 / 状態: 未着手

このファイルは別セッションで作業するための引き継ぎメモ。単独で読んで作業を始められるように
経緯も含めて書いてある。背景の詳細は `docs/notes/include-investigation.md`。

## 経緯

1. `02_simplecube/simplecube_app.cpp` の `std::cosf` / `std::sinf` について
   「C の拡張ではないか」という疑問から調査を開始した
2. 実際は `cosf`/`sinf` は C99 標準、`std::cosf` も libstdc++ が提供しており、
   **書き方自体は問題なかった**。ただし当該ファイルは `<cmath>` を include しておらず、
   `glm/glm.hpp` → `glm/detail/_fixes.hpp` 経由で暗黙に入っていた（`-H` で特定）
3. `<cmath>` を明示追加し、`std::cosf` → `std::cos` に変更（済・コミット未確認）
4. 同種の問題が他にもあるはずなので clang-tidy を導入したところ、
   **9ファイル 30箇所**で同じ構造の暗黙依存が見つかった

つまりこれは単発のバグ修正ではなく、**プロジェクト規約「推移的インクルードは行わず、
ヘッダファイルは明示的に指定する」と実態のずれを埋める作業**である。

## なぜ直すのか

現状 Windows / Kubuntu どちらでもビルドは通る。壊れているわけではない。
直す理由は、**動くかどうかが glm や標準ライブラリの実装バージョンに左右される状態を
やめること**。今回の `<cmath>` はたまたま glm が入れてくれていただけで、glm 側の
リファクタ一つで両環境とも壊れる。

## やること

`clang-tidy` が指摘する30箇所に、対応するヘッダを明示的に `#include` する。

### 対象（2026-07-30 時点、`run-clang-tidy -p build/debug -quiet -j 8` の結果）

| ファイル                                    | 件数 | 不足している名前                                                                |
| ------------------------------------------- | ---- | ------------------------------------------------------------------------------- |
| `lib/stage1/core/swapchain.cpp`             | 5    | `uint32_t`, `std::vector`, `std::max`, `UINT32_MAX`, `UINT64_MAX`               |
| `lib/stage1/core/shader_loader.cpp`         | 5    | `std::filesystem::path`, `std::ios`, `std::runtime_error`, `size_t`, `uint32_t` |
| `lib/stage1/core/vulkan_context.cpp`        | 4    | `std::vector`, `std::shared_ptr`, `std::make_shared`, `std::make_unique`        |
| `lib/stage1/core/glfw_surface_provider.cpp` | 4    | `GLFWwindow`, `glfwCreateWindowSurface`, `glfwGetFramebufferSize`, `uint32_t`   |
| `02_simplecube/simplecube_app.cpp`          | 4    | `std::vector`, `assert`, `memcpy`, `DepthBuffer`                                |
| `01_triangle/triangle_app.cpp`              | 3    | `std::vector`, `offsetof`, `ImageLayoutTransition`                              |
| `lib/stage1/core/resource_uploader.cpp`     | 2    | `std::vector`, `std::move`                                                      |
| `lib/stage1/core/command_buffer.cpp`        | 2    | `VulkanContext`, `ImageLayoutTransition`                                        |
| `lib/stage1/core/image_resource.cpp`        | 1    | `uint32_t`                                                                      |

対応ヘッダの目安: `std::vector` → `<vector>` / `std::max` → `<algorithm>` /
`uint32_t`, `UINT32_MAX`, `UINT64_MAX` → `<cstdint>` / `size_t`, `offsetof` → `<cstddef>` /
`assert` → `<cassert>` / `memcpy` → `<cstring>` / `std::move`, `std::make_unique` → `<utility>`,
`<memory>` / `std::runtime_error` → `<stdexcept>` / `std::ios` → `<ios>` または `<fstream>` /
`GLFWwindow` 等 → `<GLFW/glfw3.h>` / プロジェクト型 → 対応する `core/*.h`

### 手順

```fish
# 現状確認
run-clang-tidy -p build/debug -quiet -j 8

# ファイル単位で確認
clang-tidy -p build/debug --quiet lib/stage1/core/swapchain.cpp

# 自動修正（include 追加は機械的なので概ね安全。ただし必ず差分を見る）
clang-tidy -p build/debug --fix lib/stage1/core/swapchain.cpp
git diff
```

### 注意

- **`--fix` は挿入位置・ヘッダ選択が必ずしも意図どおりではない。** `<bits/...>` のような
  内部ヘッダを提案してくることがある。差分を必ず確認する
- インクルード順は `.clang-format` に従う（Google style ベース）。修正後に
  `clang-format -i <file>` をかけると並び替えられる
- **`glm/` と `vulkan/` は `.clang-tidy` で除外済み**（傘ヘッダ構成のため誤検知が出る）。
  この除外を外すと 74件に膨れるので触らない
- 修正後は必ずビルドして通すこと:

```fish
  cmake --build --preset debug
```

- **写経プロジェクトなので、AI に一括修正させるより1ファイルずつ理由を確認しながら進める方が
  学習価値が高い**。「なぜこのヘッダなのか」を都度確認したい

### 完了条件

```fish
run-clang-tidy -p build/debug -quiet -j 8 | grep misc-include-cleaner
```

が空になり、`cmake --build --preset debug` が通ること。

## この作業に含めないもの（別件として残す）

同じ clang-tidy 実行で出ている以下は本タスクの対象外:

- `clang-analyzer-deadcode.DeadStores` 5件 — 未使用ローカル変数
  （`triangle_app.cpp:132`, `simplecube_app.cpp:272,359`, `resource_uploader.cpp:28,63`）
- `performance-avoid-endl` 4件 — `std::endl` → `'\n'`
  （`01_triangle/main.cpp:129`, `02_simplecube/main.cpp:125`, `vulkan_context.cpp:24,30`）
- `bugprone-narrowing-conversions` 3件
  （`simplecube_app.cpp:144,147` の int→float、`shader_loader.cpp:19` の size_t→streamsize）
- constexpr 定数の表記統一（`PI` vs `kAssetDirs`）— 未決

**対応済み（2026-07-30）**: `readability-identifier-naming` 4件は解消。
`mainEntryPoint()` → `MainEntryPoint()`、`run()` → `Run()` にリネームし、
改名不可の `wWinMain`（Win32 が名前を固定）には `// NOLINTNEXTLINE(readability-identifier-naming)`
を付与した。あわせて `01_triangle/main.cpp` を `.clang-format` 準拠に整形。
