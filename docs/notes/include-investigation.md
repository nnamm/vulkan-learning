# インクルード調査の道具箱（`-H` / `/showIncludes`）

「この環境ではビルドできるのに、別の環境では `error: 'X' is not a member of 'std'`」——
その多くは**推移的インクルード**（自分では書いていないヘッダが、別のヘッダ経由で入ってきている）
が原因。推測で当てにいくのではなく、コンパイラにインクルード木を吐かせて特定する。

## 1. 何が起きているのか

`#include` は単純なテキスト展開なので、`a.h` が `<cmath>` を include していれば、`a.h` を
include した `.cpp` でも `std::cos` が使える。**書いていないのに使える**状態になる。

これが環境差になる理由:

- 標準ライブラリの実装（MSVC STL / libstdc++ / libc++）ごとに、ヘッダ間の依存が違う
- サードパーティライブラリのバージョンによっても変わる（今日入っていたものが明日消える）

つまり推移的インクルードへの依存は、**動くかどうかがコンパイラとライブラリのバージョンに
左右される暗黙の依存**になる。だから本プロジェクトの規約は「推移的インクルードは行わず、
ヘッダファイルは明示的に指定する」。

## 2. `-H` の読み方（GCC / Clang）

`-H` を付けると、**開いたヘッダを入れ子構造で stderr に出力**する。

```
g++ -std=c++20 -H -fsyntax-only demo.cpp
```

```
. /usr/include/c++/15/vector
.. /usr/include/c++/15/bits/requires_hosted.h
... /usr/include/x86_64-linux-gnu/c++/15/bits/c++config.h
.... /usr/include/x86_64-linux-gnu/c++/15/bits/os_defines.h
..... /usr/include/features.h
```

- **先頭のドットの数 = インクルードの深さ**。`.` が1つなら `.cpp` が直接 include したもの、
  2つならそのヘッダが include したもの、と続く
- 上に向かって「自分よりドットが少ない最初の行」を辿ると、**親（誰が呼んだか）が分かる**
- 出力は **stderr**。ファイルに保存するときは `2> inc.txt`
- 末尾に `Multiple include guards may be useful for:` というリストが付く。これは
  「インクルードガードを付けると速くなるかも」という別件の助言なので、調査時は無視してよい

補助オプション:

| オプション | 意味 |
| --- | --- |
| `-fsyntax-only` | 構文解析だけして `.o` を作らない。調査時はこれを付けると速い＆ゴミが出ない |
| `-M` | インクルードした全ヘッダを**フラットな一覧**で出す（system header 含む） |
| `-MM` | 同じだが **system header を除く**。「プロジェクト内の依存」だけ見たいとき |
| `-E` | プリプロセス結果そのものを出す。マクロ展開まで追いたい最終手段 |

`-H` は木構造（誰が誰を呼んだか）、`-M`/`-MM` は一覧（何が入ったか）。
**「なぜ入ったのか」を知りたいときは `-H`。**

Clang も `-H` が同じ形式で使える。`--trace-includes` は `-H` の別名。
**MSVC は `/showIncludes`**（`/showIncludes:user` で system header を除外）。

## 3. 実際の手順（このプロジェクトでの例）

### Step 1. 実際のコンパイルコマンドを手に入れる

自分でオプションを組み立て直すと `-I` の抜けで結果が変わる。CMake が出力する
`compile_commands.json` から**実物**を取り出すのが確実。

```fish
python3 -c "
import json
for e in json.load(open('build/debug/compile_commands.json')):
    if 'simplecube_app.cpp' in e['file']:
        print(e['command']); break
"
```

### Step 2. `-H -fsyntax-only` を足して実行し、出力を保存

```fish
# 取り出したコマンドに -H -fsyntax-only を追加し、-o と -c は外す
cd build/debug
/usr/bin/c++ -DGLM_FORCE_DEPTH_ZERO_TO_ONE -DGLM_FORCE_RADIANS \
  -I../../lib/stage1 \
  -isystem /home/nnamm/vulkan/default/x86_64/include \
  -isystem ./vcpkg_installed/x64-linux/include \
  -std=gnu++20 -H -fsyntax-only ../../02_simplecube/simplecube_app.cpp 2> inc.txt
```

### Step 3. 目的のヘッダを探し、親チェーンを復元する

```fish
grep -n cmath inc.txt
```

親を自動で辿るなら awk（`cmath` の部分を調べたいヘッダ名に変える）:

```fish
awk '/^\.+ / { d = index($0, " ") - 1; stack[d] = $2; if ($2 ~ /cmath$/) { for (i = 1; i <= d; i++) print i ": " stack[i]; exit } }' inc.txt
```

出力（2026-07-30 に実際に得られた結果）:

```
1: 02_simplecube/simplecube_app.h
2: vcpkg_installed/x64-linux/include/glm/glm.hpp
3: vcpkg_installed/x64-linux/include/glm/detail/_fixes.hpp
4: /usr/include/c++/15/cmath
```

## 4. 今回の実例 — `std::cosf` はどこから来ていたか

`02_simplecube/simplecube_app.cpp` は `<cmath>` を include していないのに `std::cosf` が
使えていた。上の手順で辿った結果、`glm/detail/_fixes.hpp`（glm 1.0.3）の1行目が
`#include <cmath>` だった。`min`/`max` マクロを `#undef` するためのヘッダで、
その副産物として `<cmath>` が必ず入る。

分かったこと:

- `cosf` / `sinf` は **C99 標準**の `<math.h>` 関数。拡張ではない
- `std::cosf` も C++ の `<cmath>` synopsis に含まれ、libstdc++ も
  `using ::cosf;` で提供している（`/usr/include/c++/15/cmath:1862`）。MSVC 専用の書き方ではない
- なので MSVC でも Kubuntu でも通っていたのは**同じ理由（glm 経由）**。ただし
  glm 側の実装変更で壊れる暗黙依存なので、`#include <cmath>` を明示した
- C++ では `std::cos` / `std::sin` のオーバーロードに `float` を渡せば `float` 版が選ばれ、
  libstdc++ の `cos(float)` は `__builtin_cosf` を呼ぶ。**生成コードは同一**なので、
  サフィックス無しに寄せて損はない

## 5. 予防側 — clang-tidy の導入（2026-07-30）

使った名前のヘッダは自分で書く、が原則。加えて機械的な検査を入れた。

### 導入したもの

```
sudo apt install clang-tidy clangd
```

`clang-tools-21` に含まれる **`clang-include-cleaner-21` は最初から入っていた**。
clang-tidy 抜きで単体調査するならこれが手軽:

```fish
clang-include-cleaner-21 -p build/debug --print=changes \
  --ignore-headers='glm/.*,vulkan/.*' 02_simplecube/simplecube_app.cpp
```

（単体ツールは **カンマ区切り**。clang-tidy 側は **セミコロン区切り**。ここが違う）

### 設定ファイルは3つに分かれる

| ファイル | 読むツール | 用途 |
| --- | --- | --- |
| `.clang-format` | clang-format | 整形 |
| `.clangd` | clangd（LSP）のみ | エディタ挙動・clangd 内蔵 include-cleaner |
| `.clang-tidy` | clang-tidy **と** clangd | チェック選択・命名規約 |

`.clangd` は clang-tidy の設定ファイルではない。`.clangd` の
`Diagnostics.Includes.IgnoreHeader` は **clangd 内蔵**の include-cleaner 向けで、
clang-tidy の `misc-include-cleaner` には効かない。同じ除外を両方に書く必要がある。

neovim で同じ指摘が二重に出るなら `.clangd` 側で片方を止める:

```yaml
Diagnostics:
  ClangTidy:
    Remove: [misc-include-cleaner]
```

### 実測で決めたこと（推測で決めなかった点）

- **`misc-include-cleaner.IgnoreHeaders` の区切りはセミコロン。** カンマは黙って無視される。
  `glm/.*,vulkan/.*` → 74件（フィルタ不発）、`glm/.*;vulkan/.*` → 4件
- **正式なオプション名は `--dump-config` で確認できる。**
  `clang-tidy --checks='-*,misc-include-cleaner' --dump-config`
  → `IgnoreHeaders` / `MissingIncludes` / `UnusedIncludes` / `DeduplicateFindings`
- **`HeaderFilterRegex` は明示が必要。** 既定ではヘッダ内の警告が report されない
- **匿名 namespace のファイルスコープ変数は `GlobalVariable` 扱い**（`StaticVariable` ではない）。
  `g_assetRoot` を通すには `GlobalVariablePrefix: 'g_'`
- **`IgnoreFailedSplit: true` は `A`..`H` や `PI` を抑止しない。** 単文字・全大文字の
  ローカル定数を許すには `LocalConstantCase: aNy_CasE` / `ConstexprVariableCase: aNy_CasE`

### 使い方

```fish
clang-tidy -p build/debug --quiet 02_simplecube/simplecube_app.cpp   # 単一ファイル
run-clang-tidy -p build/debug -quiet -j 8                            # 全ファイル
clang-tidy -p build/debug --fix 02_simplecube/simplecube_app.cpp     # 自動修正
```

### 導入時点の検出結果（46件、誤検知ゼロ）

| チェック | 件数 | 備考 |
| --- | --- | --- |
| `misc-include-cleaner` | 30 | 9ファイルに分散。→ `docs/notes/task-misc-include-cleaner.md` |
| `clang-analyzer-deadcode.DeadStores` | 5 | 未使用ローカル変数 |
| `performance-avoid-endl` | 4 | `std::endl` → `'\n'` |
| `bugprone-narrowing-conversions` | 3 | int→float 2件、size_t→streamsize 1件 |
| `readability-identifier-naming` | 4 | `mainEntryPoint` / `run`（PascalCase 違反） |

**`<cmath>` の件は単発の見落としではなく、9ファイルに散らばった同じ構造の問題だった。**
規約と実態に系統的なずれがあったということ。

### 命名チェックが掘り出した副産物

`readability-identifier-naming` を有効にする過程で、CLAUDE.md に書かれていなかった規約と、
実態の不整合が判明した:

- `g_` プレフィックス（グローバル変数）、`lower_case`（名前空間）が未文書だった → CLAUDE.md に追記
- **constexpr 定数の表記が不統一**（`PI` と `kAssetDirs`）。どちらに寄せるか**未決**なので
  今は無検査にしている。`.clang-format` が Google style ベースなので `kPi` に寄せるのが
  筋は通るが、`PI` は書籍由来
- `mainEntryPoint()` / `run()` が camelCase。同じファイルの `GetExecutableDir()` は
  PascalCase なので、これは規約からの逸脱（未修正）

## 6. 結局いちばん効くもの

「別の環境でビルドしてみる」こと。Windows と Kubuntu の2環境で写経している構成
そのものが、この種の暗黙依存を早期に炙り出す仕組みになっている。
clang-tidy はそれを待たずに机上で炙り出すための道具。
