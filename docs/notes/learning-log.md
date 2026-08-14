# 学習ログ

> 写経の進捗・現在地・次にやることを記録するログ。Windows / Kubuntu の2環境をまたいで
> 学習コンテキストを引き継ぐための「共有メモリ」。AIセッション開始時はまず最新エントリを読むこと。
>
> 運用ルール:
>
> - 新しいエントリを**上に**追加する（日付は `YYYY-MM-DD`）
> - 各エントリは「やったこと / 現在地 / 次にやること / 気づき」の4点セット
> - 写経がひと区切りついたタイミングで追記する（毎日でなくてよい）

---

## 2026-08-14（2）

### やったこと

**残っていた clang-tidy の指摘10件を解消した。** 前回の「次にやること」3番。
ブランチ `refactor/resolve-clang-tidy-findings`、理由ごとに3コミット（PR #15）。

- **未使用ローカル8件を削除**（DeadStores 5件 + `-Wall` で追加検出した3件）。
  多くは処理がライブラリ側に畳まれたときの取り残しだった —
  `OnDrawFrame()` の `device` はフェンス待ちが `VulkanContext::AcquireNextImage()` に、
  `ResourceUploader::SubmitAndWait()` の `pool` はコマンドバッファ確保が
  `VulkanContext::CreateCommandBuffer()` に移っている。`simplecube` の `shaderStages[]`（14行）は
  `GraphicsPipelineBuilder::AddShaderStage()` に置き換わったまま残っていた
- **`std::endl` → `'\n'` 2件**。`std::cerr` は unit-buffered なのでフラッシュの有無で
  出力タイミングは変わらない
- **narrowing 3件を `static_cast` で明示**。オプションを `false` にする案は採らなかった
  （3件のために意図の記録を捨てることになるため）
- **実測**: `run-clang-tidy` **10件 → 0件**、`clang-format --dry-run --Werror` 変更7ファイル OK、
  15TU を `compile_commands.json` の実コマンド + `-Wall -Wextra` でコンパイル通過。
  ビルドと各アプリの実行も確認済み

**`-Wall` / `-Wextra` はビルドに入れない、と決めた（対応なし）。** 今回 `-Wall` を一時的に足して
初めて見えた3件は、`vulkan_context.cpp` のデバッグコールバックの未使用パラメータ
（`severity` / `type` / `pUserData`）で、Vulkan が決めたシグネチャ由来。直しても写経の理解は進まず、
`[[maybe_unused]]` を足すか警告を切るかという話にしかならない。**Vulkan を知るという本質から
逸れる**ため、ここで線を引く。将来また `-Wall` を思いついたときに、この判断を検討し直さないこと。

### 現在地: 4章完了・振り返り中。5章は未着手（前回から変わらず）

`run-clang-tidy` は**0件**。7/30 の導入時点から数えて46件すべてが片付いた。

### 次にやること

1. **5章の写経を開始する**（最優先。仮説2つの検証も継続）
   - 深度バッファの遷移が `lib` 側に入るか
   - `VulkanContext::SubmitAndWait()` の呼び出し元が現れるか
2. **`Swapchain::Recreate()` の2回目以降のリーク**（`stage1-map.md` §7）。リサイズ対応時に必ず踏む
3. **ヘッダの扱いを決める**（前回4番から引き続き未決）
   - `HeaderFilterRegex` がサブディレクトリを含むパスにマッチしないため、
     `lib/stage1` 配下のヘッダは1つも検査されていない
   - 広げた場合の増分は実測済み: **+5件のみ**
   - **ただし regex 拡大では「ヘッダの自己完結性」は解決しない。**
     `misc-include-cleaner` は main file しか見ないため、別の道具立てが要る
4. Windows 環境でもビルドと clang-tidy を通す

### 気づき

- **「なぜ使っていない変数を宣言したのか」という写経中の疑問は正しかった。** 関数の先頭で
  宣言しきるのは **C89 の言語制約**（ブロック先頭以外に宣言を書けなかった）の名残で、C99 と C++ は
  最初からその制約を持たない。今の指針（C++ Core Guidelines ES.21/ES.22）は「初めて使う場所で、
  できるだけ狭いスコープで」。**C++ に詳しくないから判断できないとスルーした疑問のほうが、
  実は現在の合意に沿っていた**
- **同じ形が11回あるなら、それは11回の判断ではなく1回のひな形。** 上流サンプル
  （`techmadot/vulkan-practical-guide-samples`）を実際に取得して数えたところ、
  **全11サンプルの `OnDrawFrame()` が一字一句同じ3行で始まり、11個すべてで `device` が未使用**だった。
  1箇所だけなら「筆者の意図」を読む価値があるが、全数一致なら読むべきは元になった型のほう。
  リポジトリはコミット1本（`initial commit`）で履歴がないため、畳んだ経緯そのものは追えない
- **検出器ごとに「見ていない範囲」が違う**（8/13 の再演だが、今回は具体的な件数で出た）。
  `-Wall` だけが拾った3件のうち2件は**参照束縛**（`auto& vulkanCtx`）で、
  DeadStores は「格納」を見る検査なので原理的に対象外。**clang-tidy が0件でも、同種の負債は残る**
- **`bugprone-narrowing-conversions` は定数を畳んで、float で正確に表せる値なら黙る**（試験ファイルで実測）。
  `kPi * 2.0f / sliceCount`（`const int` 48）が鳴らず `stack * stackStep`（ループ変数）が鳴るのは
  設定の不統一ではない。`const int 16777217` のように float で表せない定数には
  **値を名指しした別のメッセージ**が出る
- **道具を足すほど「見えるが直す価値の薄いもの」の比率が上がる。** 検出器を有効にする判断とは別に、
  **どこで止めるかを決めて記録しておく**必要がある。決めずに放置すると、同じ検討を思いつくたびに
  やり直すことになる（今回の `-Wall` 対応なしは、その記録のつもり）

---

## 2026-08-14

### やったこと

**`docs/notes/archive/` を新設し、役目を終えた記録を移した。**
前回の「次にやること」5番。

- `task-misc-include-cleaner.md` を `docs/notes/archive/` に移動。完了タスクの記録で、
  要点は learning-log.md 側にも残っているため、現役のノートと同じ棚に置く必要がなかった
- **参照の張り替えは2種類に分けた。** `CLAUDE.md` の学習ノート一覧と `include-investigation.md`
  の逆引き表は「今の場所」を指すべき生きた参照なので新パスに更新。一方 learning-log.md の
  過去エントリ（7/30・8/12 など）が書いていた旧パスはそのままにした
  — ログは「書いた時点の観測」を残す場所で、過去の記述を今の状態に合わせて書き換える対象ではない

### 現在地: 4章完了・振り返り中。5章は未着手（前回から変わらず）

### 次にやること

1. **5章の写経を開始する**（最優先。仮説2つの検証も継続）
   - 深度バッファの遷移が `lib` 側に入るか
   - `VulkanContext::SubmitAndWait()` の呼び出し元が現れるか
2. **`Swapchain::Recreate()` の2回目以降のリーク**（`stage1-map.md` §7）。リサイズ対応時に必ず踏む
3. 残りの clang-tidy 指摘 — DeadStores 5件 / `performance-avoid-endl` 2件 / narrowing 3件。
   理由が3系統なので1ブランチ3コミットが妥当
4. **ヘッダの扱いを決める**（旧5番から引き続き未決）
   - `HeaderFilterRegex` がサブディレクトリを含むパスにマッチしないため、
     `lib/stage1` 配下のヘッダは1つも検査されていない
   - 広げた場合の増分は実測済み: **+5件のみ**
   - **ただし regex 拡大では「ヘッダの自己完結性」は解決しない。**
     `misc-include-cleaner` は main file しか見ないため、別の道具立てが要る
5. Windows 環境でもビルドと clang-tidy を通す

### 気づき

- **「参照を張り替える」と「記録を書き換える」は別の作業。** 同じ文字列
  `docs/notes/task-misc-include-cleaner.md` が複数箇所にあっても、それが「今どこにあるか」を
  指しているのか「あの時どこにあったか」を記録しているのかで扱いが変わる。前者（CLAUDE.md、
  逆引き表）は更新し、後者（ログの過去エントリ）は残した。**grep で全箇所を洗い出したあと、
  機械的に一括置換しないのが正しい**場面もある

---

## 2026-08-13（2）

### やったこと

**整形規約を確定して全ファイルに適用し、`GLFW_INCLUDE_VULKAN` を CMake に移した。**
前回の「次にやること」7番と4番。2ブランチ・3コミット（PR #11 / #12）。

- **`DerivePointerAlignment` を固定した**（7番）— `false` + `PointerAlignment: Left`。
  決める前に全31ファイルを両設定で整形して出力を突き合わせ、**差分0行**を確認した。
  「実態は揃っているが偶然」という前回の見立てが実測で裏付けられた形
- **未整形だった14ファイルに `.clang-format` を適用した**（今回発見した別件）。442/470行。
  素の Google style（2スペース）のままだった。**うち12がヘッダ**
- **`GLFW_INCLUDE_VULKAN` を各章の `target_compile_definitions` に移した**（4番）。
  3箇所の `#define` を削除。`project_options` は採らなかった
  — 一貫性のため `GLM_FORCE_*` も動かすことになり、変更が不必要に広がるため
- **順序を「7番 → 整形 → 4番」にした。** 4番が触る `glfw_surface_provider.h` が未整形で、
  先に触ると意味のある3行が再インデントに埋もれる。8/12 に経験したのと同じ状況を、
  今回は**踏む前に避けた**

### 現在地: 4章完了・振り返り中。5章は未着手（前回から変わらず）

整形は全31ファイルに適用済み。`run-clang-tidy` の残りは**10件で前回と同数・同内容**
（DeadStores 5 / `performance-avoid-endl` 2 / narrowing 3）。今回は一切触っていない。

### 次にやること

1. **5章の写経を開始する**（最優先。仮説2つの検証も継続）
   - 深度バッファの遷移が `lib` 側に入るか
   - `VulkanContext::SubmitAndWait()` の呼び出し元が現れるか
2. **`Swapchain::Recreate()` の2回目以降のリーク**（`stage1-map.md` §7）。リサイズ対応時に必ず踏む
3. 残りの clang-tidy 指摘 — DeadStores 5件 / `performance-avoid-endl` 2件 / narrowing 3件。
   理由が3系統なので1ブランチ3コミットが妥当
4. **ヘッダの扱いを決める**（前回5番）。今回**問題の捉え方が変わった**（下の気づき参照）
   - `HeaderFilterRegex` がサブディレクトリを含むパスにマッチしないため、
     `lib/stage1` 配下のヘッダは1つも検査されていない
   - 広げた場合の増分は実測済み: **+5件のみ**
   - **ただし regex 拡大では「ヘッダの自己完結性」は解決しない。**
     `misc-include-cleaner` は main file しか見ないため、別の道具立てが要る
5. **`docs/notes/archive/` を作り、役目を終えた記録を移す**（今回追加）。
   `task-misc-include-cleaner.md` は完了タスクの記録で、要点はこのログにもある。
   **現役のノートと同じ棚に並んでいると、どれが今の参照先か分からなくなる**
6. Windows 環境でもビルドと clang-tidy を通す

### 気づき

- **「揃っている」と「揃うよう強制されている」は別。** `DerivePointerAlignment: true` は
  `*` の位置をファイルの中身から推定する設定で、全ファイルが偶然左寄せだっただけだった。
  1ファイル逆に書けばそれが通り、以降そのファイルは逆のまま整形され続ける。
  **偶然の一致は最初の1件で崩れる** — 8/13 に `ClassConstantCase` の穴を
  「該当コードは現在ゼロだが」塞いだのと同じ形の判断
- **効いていることの証明は、外して壊れることでしか取れない。** `GLFW_INCLUDE_VULKAN` を
  CMake に移したあと、ビルドが通ることは確認できる。だが**それは定義が効いていなくても成り立つ**
  （`glfwCreateWindowSurface` を呼ぶのは1ファイルだけなので、他が壊れていても気づけない）。
  定義だけを外して `‘glfwCreateWindowSurface’ was not declared in this scope` が出ることを
  見て初めて、その定義が仕事をしていると言える。**必要条件を十分条件と読み違えない**
- **`#define` では表現できない要求がある。** `GLFW_INCLUDE_VULKAN` の本当の条件は
  「`glfw3.h` が最初に展開される時点で `vulkan.h` を通過済みであること」で、
  これは**ファイルの性質ではなく include 順序の性質**。だからどのファイルに書いても保証にならない。
  実際 `glfw_surface_provider.cpp` は `glfwCreateWindowSurface` を呼ぶのに自分では定義せず、
  自ヘッダが先に読まれることに依存していた。**全 TU で順序と無関係に成立する場所は
  コマンドラインしかない**
- **道具の「見ていない範囲」は、道具ごとに別々に漏れる。** clang-tidy はヘッダを
  `HeaderFilterRegex` で外していて、clang-format はヘッダに一度も実行されていなかった。
  **別々の理由で、同じヘッダ群が両方から漏れていた**。5番を「clang-tidy の設定の問題」ではなく
  「ヘッダが道具の対象外」と捉え直すと、今回の整形漏れもその一部だった。
  **道具を増やすたびに「これは何を見ていないか」を確かめる**（8/12 の
  「`misc-include-cleaner` は main file しか見ない」の再演）
- **「整形のみ」は証明できる。** 全ファイルから空白・タブ・改行を除去してハッシュを比較すれば、
  一致＝空白以外は変わっていない、が言い切れる。目視で確かめる必要がなくなる。
  なお `git diff -w` では判定できない — カラム制限が80から100に変わって
  折り返されていた行が結合されると、空白以外の差分として残るため
- **ノートも「書いた時点の観測」であって真実ではない。** 今回、環境について残されていた記録に
  誤りが見つかり、測り直して否定された。**測り直せるものは、記録を信じる前に測り直す**
  （7/30「ツールの設定はドキュメントより実測」が、自分のノートにも当てはまるという話）

---

## 2026-08-13

### やったこと

**constexpr 定数の表記統一（前回の「次にやること」6番）を片付けた。**
ブランチ `refactor/constexpr-naming-convention`、4コミット。

- **`.clang-tidy` に定数のルールを入れた** — `ConstexprVariableCase: CamelCase` +
  `ConstexprVariablePrefix: 'k'`。`PI` → `kPi` にリネーム。`LocalConstantCase: aNy_CasE` は
  維持したので、Cube 頂点の `A`..`H` は数学記法のまま残る
- **`ClassConstantCase` の穴を塞いだ** — 検証中に発見。`static const` メンバは
  `ClassConstant` が未設定だと **`GlobalVariable` に落ちて `g_otherConstant` を提案してくる**。
  該当コードは現在ゼロだが、最初の1件が誤った名前を指されないように設定した
- **`MaxInflightFrames` を `static constexpr` にした**（→ `kMaxInflightFrames`）。
  詳細は下の気づき。ブランチ名の射程外だったが、望ましい形を採る判断
- **CLAUDE.md のコーディング規約を「整形 / 命名 / その他 / 検査」に分割した。**
  旧版は `.clang-format` の行を先頭に置いた1つの箇条書きで、末尾に
  「上記は `readability-identifier-naming` で機械化済み」と書いてあった。
  この「上記」には整形の行も `#pragma once` の行も入っており、**文が事実と違っていた**
- ブランチ名は当初案の `chore/determine-how-to-constant` から変更。`determine-` は
  作業中の思考を指していてマージ後に何が入ったか読めないこと、識別子のリネームは
  `chore` ではなく `refactor` であることが理由

### 現在地: 4章完了・振り返り中。5章は未着手（前回から変わらず）

`run-clang-tidy` の残りは10件で前回と同数・同内容。**ヘッダまで検査対象を広げても +5件**
であることを実測済み（下記「次にやること」5番）。

### 次にやること

1. **5章の写経を開始する**（最優先。仮説2つの検証も継続）
   - 深度バッファの遷移が `lib` 側に入るか
   - `VulkanContext::SubmitAndWait()` の呼び出し元が現れるか
2. **`Swapchain::Recreate()` の2回目以降のリーク**（`stage1-map.md` §7）。リサイズ対応時に必ず踏む
3. 残りの clang-tidy 指摘 — DeadStores 5件 / `performance-avoid-endl` 2件 / narrowing 3件
4. `GLFW_INCLUDE_VULKAN` を CMake 側の定義に移す
5. **ヘッダの扱いを決める**（旧5番を拡張）。今回2つが同じ根だと分かった
   - `HeaderFilterRegex` が**サブディレクトリを含むパスにマッチしない**ため、
     `lib/stage1` 配下のヘッダは1つも検査されていない（章ディレクトリ直下のヘッダは対象）
   - 広げた場合の増分を実測済み: **+5件のみ**。CRTP コンストラクタ 3件
     （`gpu_resource_base.h` / `buffer_resource.h` / `image_resource.h`）、
     `performance-enum-size` 1件（`AssetType` は `uint8_t` で足りる）、
     命名 1件（`GetWindowSystemExtensions` — `std::function` 型のメンバ変数なのに関数風の名前）
   - **ただし regex 拡大では「ヘッダの自己完結性」は解決しない。** `misc-include-cleaner` は
     main file しか見ないため、別の道具立て（各ヘッダを単独で include するだけの .cpp を生成する等）が要る
6. Windows 環境でもビルドと clang-tidy を通す
7. `DerivePointerAlignment: true` をどうするか。今は `*` の位置を**強制していない**
   （実態は `char* p` の左寄せに揃っているが偶然）。強制するなら `false` + `PointerAlignment: Left`

### 気づき

- **「◯◯ style に従う」は、その規約が何を対象にしているかを読むまで決定ではない。**
  Google style の `k` プレフィックスは「**静的記憶域期間**を持つ定数」に必須で、
  それ以外の記憶域期間には**任意**と明記されている。`PI` も `kAssetDirs` も関数内の
  ローカルで、`constexpr` を付けても static にはならないので、**両方とも「どちらでもよい」領域**だった。
  「Google に寄せるなら `kPi`」は決め手になっておらず、結局プロジェクトが決めるしかなかった
- **宣言の形と意味がずれていることがある。** `const uint32_t MaxInflightFrames = 2;` は
  言語としては「インスタンスごとに持つ値」を宣言していたが、初期化子がリテラルで `const` なので
  **インスタンスごとに変える方法が無い**。設定できる形をしているのに設定できない状態だった
- **ドキュメントの方が先に正しいことがある。** CLAUDE.md は以前からこの定数を
  `VulkanContext::MaxInflightFrames` と書いていたが、**その式は当時コンパイルできなかった**
  （非 static データメンバを型名経由で参照している）。文書が「これは static のはず」という
  正しい直観で書かれていて、コードだけが追いついていなかった
- **「本来は◯◯」で片付けず、効き目を分けて測る。** `static constexpr` にする理由として
  4バイトの節約・コピー代入の delete 回避・定数式で使えること・インスタンス不要を挙げたが、
  シングルトンなので**実際に効いていたのは最後の1つだけ**だった。言語仕様として正しいことと、
  このコードベースで効いていることは別
- **整形と命名は別の軸で、たまたま両方に "Google" が出てくる。** `clang-format` は
  識別子の名前に一切触れない（空白・改行・整列のみ）。`.clang-format` の
  `BasedOnStyle: Google` が命名の不統一を生むことは原理的にありえない。
  ちなみに `BasedOnStyle` は165項目の初期値セットで、このプロジェクトは6項目を上書きしている。
  ベースを他に変えたときの差分を実測したら、Microsoft / LLVM で30行台、内容は
  行末コメント前の空白の個数など**3項目の好み**でしかなかった
- **このプロジェクトの命名規約は書籍由来であって Google style ではない。** 決定的なのは
  `m_` プレフィックスで、Google の private メンバは `snake_case_`。7/30 に
  「実態に合わせて」文書化したのだから当然だが、`.clang-format` の1行が
  誤解の種になっていた（今回 CLAUDE.md を分割した理由）
- **実態から起こした規約は、実態と衝突しない。** `lib/stage1` のヘッダは一度も
  検査されていなかったのに、命名違反は1件だけだった。理想から規約を作っていたら、
  検査していない領域に違反が溜まっていたはず。**規約を「あるべき姿」で書くか
  「今そうなっている姿」で書くかは、未検査領域の壊れ方を変える**
- **ツールの分類は実測で確かめる**（7/30「ツールの設定はドキュメントより実測」の再演）。
  `readability-identifier-naming` は `ConstexprVariable` を `LocalConstant` / `ClassConstant`
  より**先に**判定する。この優先順位のおかげで「constexpr は `k` 必須、ローカルの `const` は
  無検査」を両立できた。ルールを書く前に、書きかけの設定で1回走らせて確かめるのが速い
- 書籍サンプルの `.h.txt` は `.gitignore` で除外されており一度もコミットされていない。
  **原典と直接比較する手段は今のところ無い**（比較したくなる場面が今回あった）

---

## 2026-08-12（3）

### やったこと

**`misc-include-cleaner` の30件を潰し、「推移的インクルードは行わない」を実態に合わせた。**
（詳細は `docs/notes/task-misc-include-cleaner.md`。ブランチ `chore/misc-include-cleaner`）

- 9ファイルに22本の `#include` を追加。`--fix` は使わず、「なぜこのヘッダなのか」を1件ずつ確認して手で選んだ
- コミットは3つに分割 —
  ①`.clang-format` の適用のみ（6ファイル・約600行）②`#include` の追加（本題）
  ③コメントアウトされた `#include` の残骸削除
- 対象ファイルの多くが古い LLVM 既定（2スペース / 80カラム）のままだったため、整形すると
  意味のある22行が600行の再インデントに埋もれる。**「整形と意味変更を混ぜない」を実際にやってみた回**

### 現在地: 4章完了・振り返り中。5章は未着手（前回から変わらず）

`run-clang-tidy` の残りは10件で、いずれも別件として意図的に残しているもの。

### 次にやること

1. **5章の写経を開始する**（前回からの最優先。仮説2つの検証も継続）
   - 深度バッファの遷移が `lib` 側に入るか
   - `VulkanContext::SubmitAndWait()` の呼び出し元が現れるか
2. **`Swapchain::Recreate()` の2回目以降のリーク**（`stage1-map.md` §7）。リサイズ対応時に必ず踏む。
   8/12（2）のエントリで持ち越しが途切れていたので復帰させた
3. 残りの clang-tidy 指摘 — DeadStores 5件 / `performance-avoid-endl` 2件 / narrowing 3件
4. `GLFW_INCLUDE_VULKAN` を CMake 側の定義に移す（今回見つかった別件）
5. ヘッダの自己完結性の検査方法を決める（今回の30件は `.cpp` のみが対象だった）
6. constexpr 定数の表記統一（`PI` vs `kAssetDirs`）— 未決。Google style に寄せるなら `kPi`
7. Windows 環境でもビルドと clang-tidy を通す

### 気づき

- **`GLFW_INCLUDE_VULKAN` の本当の条件は「`glfw3.h` が最初に展開される時点で `vulkan.h` を
  通過済みであること」。** `glfwCreateWindowSurface` の宣言を守っているガードはこのマクロではなく
  `#if defined(VK_VERSION_1_0)` で、マクロはその条件を `glfw3.h` に自分で満たさせるスイッチにすぎない。
  だから include の順序が入れ替わると「宣言が丸ごとスキップされ、以降はインクルードガードで
  二度と展開されない」という取り返しのつかない壊れ方をする。C++ の include はテキスト展開なので、
  **「何を書いたか」だけでなく「どの順で読まれるか」で結果が変わる**
- **`misc-include-cleaner` は main file しか見ない。** ヘッダの暗黙依存は今回の30件に一度も
  現れていない。「0件になった」は「.cpp については保証できた」であって、プロジェクト全体ではない。
  **ツールが何を見ていないかを知らないと、通ったことの意味を過大評価する**
- 自ヘッダが既に include しているものを `.cpp` 側でも書く場面があった。重複に見えるが意図的で、
  型を名指しする TU が、自ヘッダがその include を持ち続けてくれることに依存すべきではない、という理屈

---

## 2026-08-12（2）

### やったこと

**Linux で検証レイヤーが一度も動いていなかったことが判明し、有効化して出た指摘を潰した。**

- **原因**: `CreateInstance()` の `#if DEBUG || _DEBUG`。MSVC は Debug 構成で `_DEBUG` を自動定義するが
  GCC/Clang は何も定義しない。**写経の初期は Windows がメイン環境だったため気づかず、
  Kubuntu に移ってから静かに無効化されていた**
- **対策**: `CMakeLists.txt` の `project_options` に `$<$<CONFIG:Debug>:DEBUG>` を追加。
  ソースの条件式は触らないので、Windows は従来どおり `_DEBUG` 経由で動く
- **出た指摘は `ERROR` が3種類。`WARNING` と `PERFORMANCE` はゼロ**
  1. `VkDescriptorSetAllocateInfo-sType-sType` — `sType` の写経ミス → 修正
  2. `vkDestroyDevice-device-05137` — `Cleanup()` の DescriptorPool 破棄漏れ → 修正
  3. `vkCmdBeginRendering-pRenderingInfo-09588` — 深度バッファのレイアウト未遷移 → **5章送り**
- 3 を保留にしたのは、5章に `DEPTH_ATTACHMENT_OPTIMAL` が出てくるため。写経の連続性を優先し、
  自前の実装を先に入れて書籍版と衝突させない判断（詳細と検証手順は `stage1-map.md` §7）

### 現在地: 4章完了・振り返り中。5章は未着手

検証レイヤーが常時有効になった。既知の違反は3の1件のみで、実行するたび指摘が出る状態。

### 次にやること

1. **5章の写経を開始する。** 併せて2つの仮説を検証する
   - 深度バッファの遷移が `lib` 側に入るか（入れば 02 は再ビルドだけで直る）
   - `VulkanContext::SubmitAndWait()` の呼び出し元が現れるか（実装はサンプルにあり、写経で飛ばしていた）
2. `docs/notes/task-misc-include-cleaner.md` の30件（**別セッションで実施予定**）
3. 残りの clang-tidy 指摘（`performance-avoid-endl` を1件消化）
4. constexpr 定数の表記統一（`PI` vs `kAssetDirs`）— 未決
5. Windows 環境でもビルドと clang-tidy を通す

### 気づき

- **無効化された検出器は、存在しないのと同じ。** 4章まで「動いている」と思っていたコードに
  仕様違反が3件あり、うち2件は数分で直る単純な抜けだった。実力の問題ではなく
  **見えていなかっただけ**。検証レイヤーを最初に有効化しておくのが、何よりコストの安い投資
- **条件コンパイルは、環境を移した瞬間に無言で死ぬ。** ビルドは通り、警告も出ず、
  「無効になったこと自体が無言」だった。デバッグ機能の有効/無効は、起動時に1行ログを出すなどして
  **外から見える状態にしておくべき**かもしれない
- **同名の別関数に注意。** `VulkanContext::SubmitAndWait(std::shared_ptr<CommandBuffer>)` を
  探していたのに `ResourceUploader::SubmitAndWait()` を見つけ、「解決した」と誤認しかけた。
  探すときは**クラス名とシグネチャまで込みで**照合する
- **サンプルとの差分照合は、行番号でなく「どの関数のどの位置か」で行う。** 同じ `TransitionLayout` が
  描画前（`FromUndefinedToColorAttachment`）と描画後（`FromColorToPresent`）の2箇所にあり、
  別々の場所を比べて食い違いと誤認した。**同じ関数が複数回呼ばれるコードでは必ず起きる**
- **1つの検出器を有効にすると、無関係な既存タスクも1件片付く。** `performance-avoid-endl` の対応を
  ついでに済ませられた。指摘が出ている場所を触るときが、そのファイルの負債を返す好機

---

## 2026-08-12

### やったこと

4章までの復習の一環として、`lib/stage1` を全ファイル精読し、ファイル単位の索引を作った。

- **`docs/notes/stage1-map.md` を新設。** `lib/stage1` の25ファイル（ヘッダ14 / 実装11）について
  「目的・主な処理・責務の境界・誰が使うか」を整理。`vulkan-mental-model.md` が「概念→コード」なのに対し、
  こちらは「コード→概念」の逆引き。5レイヤの俯瞰図、逆引き表、読む順（1フレームが流れる順）を付けた
- **精読の副産物として、未使用・未接続・実装の齟齬を洗い出した**（stage1-map.md §7）。
  推測を混ぜないよう、すべて grep と `nm` で裏を取ってから記載した
- **両章の `main.cpp` の `wWinMain` が `run()`（小文字）を呼んでいたのを `Run()` に修正。**
  7/30 の PascalCase リネームで Windows 側だけ取り残されていた。Linux は `main()` 側を通るため露見せず、
  **Windows ではコンパイルが通らない状態だった**（7/30 エントリ「Windows 側では未確認」がそのまま的中）

### 現在地: 4章完了・振り返り中。5章は未着手

`lib/stage1` の全体像はファイル単位で言語化できた。clang-tidy の残件は 7/30 から変化なし。

### 次にやること

1. stage1-map.md §7 に挙げた要修正3件（詳細は同ファイル参照）
   - `VulkanContext::AllocateDescriptorSet()` の `sType` 誤り（実害あり）
   - `VulkanContext::SubmitAndWait()` — 宣言のみで実装が無い。実装するか宣言を消すか決める
   - `Swapchain::Recreate()` の2回目以降のリーク（リサイズ対応時に必ず踏む）
2. `docs/notes/task-misc-include-cleaner.md` の30件を解消する（**別セッションで実施予定**）
3. 残りの clang-tidy 指摘12件（DeadStores 5 / `std::endl` 4 / narrowing 3）
4. constexpr 定数の表記統一（`PI` vs `kAssetDirs`）— 未決
5. 5章の写経を開始する
6. Windows 環境でもビルドと clang-tidy を通す（今回の `Run()` 修正の確認も兼ねる）

### 気づき

- **「写経して動いた」と「そのファイルの責務を一文で言える」は別物。** 索引を作る＝全ファイルに
  一行の要約を付ける作業なので、曖昧なまま通過していた箇所が機械的にあぶり出される。
  特に `gpu_resource_base.h`（17行）や `image_barrier.h` のような**短いファイルほど、
  なぜ在るのかを考えずに写せてしまう**。行数と理解の必要量は比例しない
- **未使用コードの多さは、写経の現在地を測る物差しになる。** `Texture2D` / `StorageImage2D` /
  `ImageLayoutTransition` の5ファクトリ / `UseRenderPass()` などが未使用のまま在るのは、
  **書籍のライブラリが先に完成形で与えられている**ため。「まだ使っていない＝これから使う章がある」と読める
- **リネームの取りこぼしは、ビルドされない分岐に溜まる。** `run()` → `Run()` が
  `#if defined(_WIN32)` の中だけ残っていた。片方の環境でビルドが通ることは、
  もう片方が壊れていないことの証明にならない（7/30 の「動いていることは依存が正しいことの証明にならない」の別版）
- **`sType` の指定ミスは型システムをすり抜ける。** `VkStructureType` は単一の enum なので、
  `VkDescriptorSetAllocateInfo` に `..._DESCRIPTOR_SET_LAYOUT_CREATE_INFO` を入れてもコンパイルは通る。
  **`sType` は「この構造体が何であるか」の自己申告**であり、C API で `pNext` を辿るために存在する。
  検証レイヤーだけが頼りなので、Validation Layer を切って開発しないこと

---

## 2026-07-30

### やったこと

4章までの振り返りの一環として、コード品質まわりを整備した（写経そのものは進めていない）。

- **`std::cosf` の疑問から推移的インクルード問題を発見。** `02_simplecube` の `cosf`/`sinf` が
  「C の拡張では?」という疑問を起点に調査。結論は「`cosf` は C99 標準、`std::cosf` も
  libstdc++ が提供しており書き方は問題なし」。ただし `<cmath>` を include しておらず、
  `glm/glm.hpp` → `glm/detail/_fixes.hpp` 経由で暗黙に入っていた（`g++ -H` で特定）
- **`.clang-tidy` を導入**（`clang-tidy` / `clangd` を apt で追加）。導入時点で46件検出、
  誤検知ゼロ。うち `misc-include-cleaner` が30件で、**同じ構造の暗黙依存が9ファイルに分散**していた
- **CLAUDE.md の命名規約を実態に合わせて修正。** 「メンバー関数は camelCase」と書いてあったが
  実コードは全て PascalCase。未文書だった `g_` プレフィックスと `lower_case` 名前空間も追記し、
  `readability-identifier-naming` で機械化。`mainEntryPoint`/`run` を PascalCase にリネームして0件に
- **Git 運用方針を CLAUDE.md に明文化。** コミットを「変更の理由」で分ける、整形と意味変更を
  混ぜない、マージは `Create a merge commit`（Squash は使わない）
- ノート2本を追加: `include-investigation.md`（`-H` の使い方と clang-tidy 設定の経緯）、
  `task-misc-include-cleaner.md`（残30件の引き継ぎメモ）
- 上記を5コミットに分割して PR #5 → `Create a merge commit` でマージ

### 現在地: 4章完了・振り返り中。5章は未着手

clang-tidy の残件42件（うち30件は推移的インクルード）。ビルドは Kubuntu で通ることを確認済み。
Windows 側では未確認（`wWinMain` の `NOLINTNEXTLINE` も Windows でのみ検証可能）。

### 次にやること

1. `docs/notes/task-misc-include-cleaner.md` の30件を解消する（**別セッションで実施予定**）
2. 残りの clang-tidy 指摘12件（DeadStores 5 / `std::endl` 4 / narrowing 3）
3. constexpr 定数の表記統一（`PI` vs `kAssetDirs`）— 未決。Google style に寄せるなら `kPi`
4. 5章の写経を開始する
5. Windows 環境でもビルドと clang-tidy を通す

### 気づき

- **「ビルドが通る」と「依存が明示されている」は別物。** 今回 MSVC でも Kubuntu でも通っていたのは
  たまたま glm が `<cmath>` を連れてきていたから。glm 側のリファクタ一つで両環境とも壊れる。
  **動いていることは、依存が正しいことの証明にならない**
- **エラーメッセージの文面を疑う前に、依存の入り口を疑う。** 「`std::cosf` が無い」と言われると
  `cosf` の書き方を疑いたくなるが、実際は include 漏れだった。`-H`（GCC/Clang）や
  `/showIncludes`（MSVC）でインクルード木を出せば、推測せずに特定できる
- **C++ に統一された命名のベストプラクティスは存在しない。** 標準ライブラリは snake_case、
  Google は PascalCase、LLVM は camelBack、Qt は camelCase。
  **唯一の基準はプロジェクト内の一貫性**。今回 PascalCase を選んだのは「正しいから」ではなく
  「このプロジェクトが既にそう選んでいたから」
- **規約は機械化して初めて実態とのずれが見える。** CLAUDE.md の命名規約は3項目が実態と違い、
  2項目が未文書だった。人間の目視では気づけていなかった
- **ツールの設定はドキュメントより実測。** `misc-include-cleaner.IgnoreHeaders` の区切りは
  セミコロンで、カンマは**エラーにならず黙って無視される**（74件 vs 4件で判明）。
  一方、単体ツールの `clang-include-cleaner` はカンマ区切り。同系統のツールでも違う
- **コミットは「大きさ」ではなく「変更の理由」で分ける。** 判断基準は
  「単独で revert できるか」「`git blame` で読む人が納得するか」。
  今回の作業は一言でいえば「整備」だが、それは結果の要約であって理由ではない

### やったこと

- 7/18〜19 にかけて **4章の写経を完了**（Cube/Sphere とも描画済み。追加のハマりや新しい学びは特になし）
- 写経環境構築のブログ記事を公開（7/4 エントリの宿題①を回収）:
  [CMake、vcpkg、Ninja、MSVC/g++でVulkan写経環境を作る](https://nnamm.work/vulkan-dev-setup-cmake-vcpkg-ninja-msvc-gpp/)
  — Windows 11 / Kubuntu 26.04 の2環境セットアップ、vcpkg.json → CMakeプリセット →
  compile_commands.json という写経の進め方5ステップをまとめた
- `vulkan-mental-model.md` を4章完了時点に最新化（AIセッション）。ステージング転送・
  ディスクリプタの配線・「×2」の理由・深度バッファ・1フレームのデータフローを §4 として追加

### 現在地: 4章完了。次は5章へ

### 次にやること

- 5章の写経を開始する
- 気が向いたら `vulkan-mental-model.md` の宿題3つ（タイムライン手描き、配線図と個数表の白紙再現、
  Fence/Semaphore の RAIIラッパー考察）を消化する

### 気づき

- 数日前の写経内容でも、ログに書かないと忘れる。**区切りがついたらその日のうちに追記する**のが
  このログの正しい運用（今回は3日遅れで思い出しながら書いた）

---

## 2026-07-17

### やったこと

- `02_simplecube` のビルドでリンクエラー（`undefined reference` / `vtable for ...`）に遭遇し、
  自力で原因を特定・解決（`buffer_resource.cpp` / `resource_uploader.cpp` にメソッドの
  **実体（定義）が書かれていなかった**）。写経あるある: 書籍が省略する暗黙の定義。GitHub に完成形の
  `lib/` があり、章ごとに手で再現するこのスタイルだと定義漏れ＝リンクエラーが起きやすい。
- `OnInitialize()` の `#if 01 / #else` ジオメトリ切り替えの意味を確認し、**Sphere ジオメトリを
  表示できるようにした**。`01` は8進数の1で常に真＝ビルド設定では切り替わらない「ソース手動トグル」。
  Sphere 側は未検証状態で、①関数名タイプミス（`CreateSphereGeometory` → `CreateSphereGeometry`）
  ②頂点ループ条件の取り違えを直して描画成功。

### 現在地: 4章 `02_simplecube` 写経中（章4.13 まで完了・Cube/Sphere ともビルド&実行 OK）

### 次にやること

- 引き続き 4章の続きを写経。区切りで本エントリを更新する。
- 気が向いたら Sphere のワインディングとカリング（`VK_CULL_MODE_BACK_BIT` /
  `FRONT_FACE_COUNTER_CLOCKWISE`）の相性を確認しておく。

### 気づき

- **ジオメトリの `for` 二重ループは「緯度 × 経度」で捉える**: 外側 `stack`(緯度) が北極→南極へ
  輪切りを1段ずつ降り、内側 `slice`(経度) がその輪を一周サンプリング。頂点はグリッドの交点、
  インデックスは隣接4点の四角形を三角形2枚に分割したもの。`k2 = k1 + (sliceCount+1)` は
  「1段下の同経度の頂点」を指す（+して1段進む）と分かると腑に落ちる。
- **ハマりどころ: ループ上限を `sliceStep`(角度 float ≈0.13) と取り違えると内側ループが1回しか
  回らず頂点不足**。インデックス側は `sliceCount+1` 頂点前提なので範囲外の未初期化頂点を読み、
  結果として何も表示されない。正しくは `slice <= sliceCount`。**「ビルドは通るが表示されない」＝
  実行時／データ側の問題**（コンパイラは配列外インデックスを検出しない）という切り分けの好例。
- 頂点フォーマット（position/normal/color）が同一なら **ジオメトリを差し替えても vert/frag は無改造で
  使い回せる**。描画パイプラインとジオメトリ生成が疎結合になっている設計の恩恵。

- **ビルドの失敗は「タイミング」で3層に切り分ける**（最初にビルドが完走したか/実行して落ちたかで二分）:
  1. コンパイル時 = 型・文法（宣言の書き間違い）
  2. **リンク時** = `undefined reference` / `vtable for X`（**実体が無い・署名不一致・ソース未登録**）
  3. 実行時 = ファイルが開けない・クラッシュ（**`.spv` やアセットのパス** — ビルドは通る点に注意）
- **リンクエラーの追い方**: ①リンク行の `.o` 一覧に対象ソースが居るか → ②`find` で `.o` の実体を確認
  （`nm` の `No such file` は「シンボルが無い」ではなく「ファイルパスが無い」の意味）→
  ③`nm -C <file>.o | grep <名前>` で `T`(定義あり)/`U`(未定義) を突き合わせる。
- **`vtable for X` が undefined = X は多態クラスなのに仮想関数の本体が欠けている**。vtable は実体を持つ
  データで、最初の非インライン仮想関数を定義する .cpp（key function）に1回だけ書き出されるため、
  本体を書き忘れると置き場所が決まらず生成されない。`Cleanup()` 等の `undefined` とセットで出るのは同根。

---

## 2026-07-04

### やったこと

- `docs/notes/vulkan-mental-model.md` の位置づけを再確認 —「写経とAIを通じた学びの道しるべ」
- 学習コンテキストを2環境（Windows / Kubuntu）で共有するため、この学習ログを新設

### 現在地: 4章 `02_simplecube` 写経中（章4.9まで完了）

**02_simplecube の実装状況:**

| 済  | 内容                                                                                      |
| --- | ----------------------------------------------------------------------------------------- |
| ✅  | `CreateCubeGeometry` — 24頂点+36インデックス、ResourceUploader 経由で DEVICE_LOCAL に転送 |
| ✅  | `CreateDescriptorSetLayout` — UBO 1個 (binding=0, VS+FS)                                  |
| ✅  | `CreateUniformBuffers` — インフライトフレーム数(2)分の `UniformBuffer`                    |
| ✅  | `CreateDescriptorSets` — `AllocateDescriptorSet` + `vkUpdateDescriptorSets`               |
| ✅  | `CreateDepthBuffer` — `D32_SFLOAT`                                                        |
| 🔶  | `CreateGraphicsPipeline` — PipelineLayout まで。**VkPipeline 本体は未着手**               |
| ⬜  | `OnDrawFrame` / `OnCleanup` — 未実装                                                      |
| ⬜  | `main.cpp` — 空ファイル（エントリーポイント未記述）                                       |
| ⬜  | シェーダーコンパイル — `cube.vert` / `cube.frag` は記述済みだが `.spv` 未生成             |

**lib/stage1 の状況:** 4章の写経に伴い `UniformBuffer` / `StagingBuffer` / `DepthBuffer` /
`DescriptorPool`（`VulkanContext::AllocateDescriptorSet`）/ `ResourceUploader` /
`GraphicsPipelineBuilder` を追加済み。`lib/stage1/**/*.h.txt` は書籍サンプルの参照用コピー（ビルド対象外）。

### 次にやること

1. `CreateGraphicsPipeline` の続き — VkPipeline 本体の生成（シェーダーステージ、頂点入力、深度テスト）
2. シェーダーを `.spv` にコンパイル（`assets/shaders/simplecube/`）
3. `OnDrawFrame`（UBO更新 → 記録 → 描画）、`OnCleanup`、`main.cpp` を書いて Cube を表示する
4. 4章完了後: ①写経環境構築のブログ化 ②`vulkan-mental-model.md` の宿題2つ
   （CPU/GPU 2本タイムライン図、Fence/Semaphore の RAIIラッパー考察）を本を読み直しながら消化

### 気づき

- `SceneConstants` の `lightDir` / `eyePosition` を `glm::mat4` と写経ミスしていた（正しくは
  `glm::vec4`、修正済み）。学びが2つ:
  1. **UBO のレイアウト不一致は Vulkan が検出しない**。CPU と GPU は「同じ構造体」を共有して
     いるのではなく「同じバイト列を別々に解釈」しているだけ。一致は自分で守る。
  2. **mat4 は「変換」、vec4 は「データ（点・方向・色）」**。「空間を動かすものか、空間の中に
     あるものか」で型が決まる。vec4 の w も同様（点=1: 平行移動を受ける / 方向=0: 受けない）。
- 学習方針:「コード→実行→表示された！→本を読み直して理解」の順で進める。宿題より写経の連続性を優先。

---

## 〜2026-06-28（これまでの歩み・要約）

- `01_triangle` 完了 — 初期化・スワップチェーン・フレーム同期・パイプラインの骨格を一周
- 4章を順次写経: 4.4 → 4.5 → 4.7（IndexBuffer / StagingBuffer / DepthBuffer 周辺）→
  4.8 UniformBuffer → 4.9 DescriptorSets（いずれもコミット済み）
- `docs/notes/vulkan-mental-model.md` 作成 — 三角形完了時点のメンタルモデルを凍結保存
