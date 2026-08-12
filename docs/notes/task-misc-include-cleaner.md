# タスク: 推移的インクルードの解消（misc-include-cleaner 30件）

作成: 2026-07-30 / 完了: 2026-08-12 / 状態: **完了**

30件すべてを解消した記録。背景の詳細は `docs/notes/include-investigation.md`。

## 結果

`run-clang-tidy -p build/debug -quiet -j 8 | grep misc-include-cleaner` が空になり、
`cmake --build --preset debug` も通過。9ファイルに22本の `#include` を追加した。

コミットは3つに分けた（`chore/misc-include-cleaner`）。

1. `.clang-format` の適用のみ（6ファイル）— 対象ファイルの多くが古い LLVM 既定
   （2スペース / 80カラム）のままで、整形すると600行規模の差分になる。意味のある22行が
   埋もれるため分離した。読まなくてよいコミット
2. `#include` の追加（9ファイル）— 本題
3. コメントアウトされた `#include` の残骸削除（`image_resource.cpp`, `swapchain.h`）

## 経緯

1. `02_simplecube/simplecube_app.cpp` の `std::cosf` / `std::sinf` について
   「C の拡張ではないか」という疑問から調査を開始した
2. 実際は `cosf`/`sinf` は C99 標準、`std::cosf` も libstdc++ が提供しており、
   **書き方自体は問題なかった**。ただし当該ファイルは `<cmath>` を include しておらず、
   `glm/glm.hpp` → `glm/detail/_fixes.hpp` 経由で暗黙に入っていた（`-H` で特定）
3. `<cmath>` を明示追加し、`std::cosf` → `std::cos` に変更
4. 同種の問題が他にもあるはずなので clang-tidy を導入したところ、
   **9ファイル 30箇所**で同じ構造の暗黙依存が見つかった

つまりこれは単発のバグ修正ではなく、**プロジェクト規約「推移的インクルードは行わず、
ヘッダファイルは明示的に指定する」と実態のずれを埋める作業**だった。

## なぜ直したのか

作業前も Windows / Kubuntu どちらでもビルドは通っていた。壊れていたわけではない。
直した理由は、**動くかどうかが glm や標準ライブラリの実装バージョンに左右される状態を
やめること**。今回の `<cmath>` はたまたま glm が入れてくれていただけで、glm 側の
リファクタ一つで両環境とも壊れる。

## 対応内容（2026-07-30 時点の指摘30件）

| ファイル                                    | 件数 | 追加したヘッダ                                                                  |
| ------------------------------------------- | ---- | ------------------------------------------------------------------------------- |
| `lib/stage1/core/swapchain.cpp`             | 5    | `<algorithm>`, `<cstdint>`, `<vector>`                                          |
| `lib/stage1/core/shader_loader.cpp`         | 5    | `<cstddef>`, `<cstdint>`, `<filesystem>`, `<ios>`, `<stdexcept>`                |
| `lib/stage1/core/vulkan_context.cpp`        | 4    | `<memory>`, `<vector>`                                                          |
| `lib/stage1/core/glfw_surface_provider.cpp` | 4    | `<GLFW/glfw3.h>`, `<cstdint>`                                                   |
| `02_simplecube/simplecube_app.cpp`          | 4    | `<cassert>`, `<cstring>`, `<vector>`, `core/image_resource.h`                   |
| `01_triangle/triangle_app.cpp`              | 3    | `<cstddef>`, `<vector>`, `core/image_barrier.h`                                 |
| `lib/stage1/core/resource_uploader.cpp`     | 2    | `<utility>`, `<vector>`                                                         |
| `lib/stage1/core/command_buffer.cpp`        | 2    | `core/image_barrier.h`, `core/vulkan_context.h`                                 |
| `lib/stage1/core/image_resource.cpp`        | 1    | `<cstdint>`                                                                     |

対応の目安（次に同じことをするとき用）: `std::vector` → `<vector>` / `std::max` → `<algorithm>` /
`uint32_t`, `UINT32_MAX`, `UINT64_MAX` → `<cstdint>` / `size_t`, `offsetof` → `<cstddef>` /
`assert` → `<cassert>` / `memcpy` → `<cstring>` / `std::move` → `<utility>` /
`std::make_unique`, `std::shared_ptr` → `<memory>` / `std::runtime_error` → `<stdexcept>` /
`std::ios` → `<ios>` / `GLFWwindow` 等 → `<GLFW/glfw3.h>` / プロジェクト型 → 対応する `core/*.h`

## 作業して分かったこと

- **`--fix` は使わず手で選んだ。** 挿入位置・ヘッダ選択が必ずしも意図どおりではなく、
  `<bits/...>` のような内部ヘッダを提案してくることがある
- **自ヘッダが既に include しているものを .cpp 側でも書く場面がある**
  （`command_buffer.cpp` の `core/image_barrier.h`、`simplecube_app.cpp` の
  `core/image_resource.h`）。これは重複ではなく意図的。型を名指しする TU が、
  自ヘッダがその include を持ち続けてくれることに依存すべきではない
- **`misc-include-cleaner` は「不足」だけでなく「使っていない include」も報告する。**
  0件になったということは、追加分に余計なものも混ざっていないことの裏付けになる
- **整形は分離できる。** `git stash` → `clang-format -i` → コミット → 内容を戻す、の順で
  「整形のみ」「意味変更のみ」の2コミットに機械的に割れる。次からは変更行だけを整形する
  `git clang-format` を使えば、そもそも混ざらない
- **`glm/` と `vulkan/` は `.clang-tidy` で除外したまま**（傘ヘッダ構成のため誤検知が出る）。
  この除外を外すと 74件に膨れるので触らない

## 残っている別件

同じ clang-tidy 実行で出ている以下は本タスクの対象外。2026-08-12 時点の実測値。

- `clang-analyzer-deadcode.DeadStores` 5件 — 未使用ローカル変数
  （`triangle_app.cpp:131`, `simplecube_app.cpp:276,363`, `resource_uploader.cpp:30,65`）
- `performance-avoid-endl` 2件 — `std::endl` → `'\n'`
  （`01_triangle/main.cpp:126`, `02_simplecube/main.cpp:126`）。
  作成時は4件と書いていたが、`vulkan_context.cpp:24,30` の分は検証レイヤー対応のブランチで解消済み
- `bugprone-narrowing-conversions` 3件
  （`simplecube_app.cpp:148,151` の int→float、`shader_loader.cpp:23` の size_t→streamsize）
- constexpr 定数の表記統一（`PI` vs `kAssetDirs`）— 未決

### 今回の作業中に見つかった別件

- **`GLFW_INCLUDE_VULKAN` を CMake 側で定義する。** このマクロは `glfw3.h` に
  `vulkan.h` を読ませるスイッチで、`glfwCreateWindowSurface` の宣言は `#if defined(VK_VERSION_1_0)`
  で守られている。つまり **`glfw3.h` が最初に展開される時点で `vulkan.h` を通過済みであること**が
  真の条件。現在は `glfw_surface_provider.h` と両 `main.cpp` の3箇所で個別に `#define` しており、
  `glfw_surface_provider.cpp` が足した `<GLFW/glfw3.h>` は「自ヘッダより後ろにある限り安全」という
  順序依存を抱えている（clang-format が main header を先頭に固定するため実害はない）。
  各章の `CMakeLists.txt` の `target_compile_definitions(... PRIVATE)` — `GLM_FORCE_*` を
  定義しているのと同じブロック — に移せば順序依存は消える。GLM と同じ扱いになり一貫もする
- **ヘッダの自己完結性は未検査。** `misc-include-cleaner` は main file の include しか
  解析しないため、`lib/stage1/**/*.h` は今回の30件に一度も現れていない。検査するには
  「ヘッダ1本だけを include する .cpp を生成してビルドする」仕掛けが要る
- **`.clang-tidy` の `HeaderFilterRegex` が `lib/stage1/core/*.h` に一致していない。**
  `'/(lib/stage[0-9]|[0-9]{2}_[a-z]+)/[^/]*\.h$'` の `[^/]*` が `core/` を跨げないため、
  マッチするのは `01_triangle/triangle_app.h` と `02_simplecube/simplecube_app.h` の2本だけ。
  「プロジェクト自身のヘッダも解析対象にする」という意図が達成できていない。
  ただし広げると `vulkan_context.h` の `MaxInflightFrames` などが
  `readability-identifier-naming` で鳴る（CLAUDE.md 的にはこちらが正しい表記）ため、
  `PublicMemberCase` の扱いを先に決める必要がある
- `resource_uploader.h:6-9` のコメントアウトされた `#include` 4行 —
  未写経の `texture_loader.h` を含み、書籍サンプルの目印の可能性があるため今回は残した
- **ヘッダは `.clang-format` 未適用のまま。** `git ls-files '*.h'` の14本中12本が非準拠
  （`image_resource.h` 100件、`vulkan_context.h` 95件、`swapchain.h` 32件など）。
  今回 `swapchain.h` の残骸削除で触れたが、整形は入れていない（本タスクの範囲外であり、
  600行規模の差分になるため）。やるなら独立したブランチで一気にかけるのが筋
