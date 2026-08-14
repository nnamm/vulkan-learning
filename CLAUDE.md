# CLAUDE.md / AGENTS.md

This file provides guidance to Claude Code and Codex when working with code in this repository.

## プロジェクト概要

書籍「Vulkan実践入門」の写経プロジェクト。C++20 / Vulkan / GLSLを使用。技術学習を主目的とするため、AIの役割はユーザサポートである。ユーザの疑問点をわかりやすく解説する。
対応OSはWindows, Linuxとする。（ユーザの環境はWindows11, Kubuntu26.04）

## 学習ノート（docs/notes/）

ユーザは Windows と Kubuntu の2環境で学習を進めるため、セッションをまたぐ学習コンテキストは
マシンローカルなメモリではなく、git 管理下の以下のファイルを正とする。

- `docs/notes/vulkan-mental-model.md` — Vulkanのメンタルモデル（オブジェクト間の関係・同期の俯瞰）
- `docs/notes/learning-log.md` — 学習ログ。写経の進捗・現在地・次にやることを記録
- `docs/notes/stage1-map.md` — `lib/stage1` のファイルマップ。各ファイルの目的・処理・責務・利用者を
  ファイル単位で逆引きする索引（メンタルモデルが「概念→コード」、こちらが「コード→概念」）
- `docs/notes/include-investigation.md` — インクルード調査の道具箱（`-H` / `/showIncludes`）と
  clang-tidy の設定経緯。環境差でビルドが通らないとき、推移的インクルードを特定する手順
- `docs/notes/archive/` — 役目を終えた記録の置き場。要点は learning-log.md 側にも残るため、
  通常のセッションでは参照不要
  （例: `task-misc-include-cleaner.md` — 完了タスクの記録。推移的インクルード30件の
  解消でやったこと・分かったこと・残した別件）

学習に関する会話を始めるときは、まず learning-log.md の最新エントリで現在地を把握すること。
写経がひと区切りついたら、ログへの追記をユーザに提案すること。

## ビルド

環境変数 `VCPKG_ROOT` が必要（vcpkg のルートパスを指す）。

```sh
# デバッグモード
cmake --preset debug
cmake --build --preset debug

# リリースモード
cmake --preset release
cmake --build --preset release
```

## プロジェクト構造

- `01_triangle/` など章ごとのディレクトリに実行ファイルを配置
- `lib/stage1/` — 基礎的なライブラリコード（common, core, render）
- `lib/stage2/` — レイトレーシング・メッシュシェーダー用ライブラリ
- `assets/shaders/` — GLSL シェーダーソース（.vert, .frag, .comp, .mesh, .rgen, .rmiss, .rchit）
- ルートの `CMakeLists.txt` の `LIB_STAGE` 変数でどのステージをインクルードするか制御

## コーディング規約

**整形と命名は別のツールが担当し、由来も別。** 混同しやすいので節を分ける。

### 整形 — `.clang-format`

`BasedOnStyle: Google` に7項目を上書き（4スペースインデント、100カラム制限ほか）。
ベースは「明示しなかった残り159項目の初期値」を決めているだけで、このプロジェクトが
Google style を採用しているという意味ではない。整形結果に不満が出たときは、ベースを
乗り換えるのではなく該当オプションを1行足して上書きする（ベース変更は159項目が
まとめて動くため、直したい1項目のために知らない項目まで動く）。
項目数は `clang-format --style=Google --dump-config` の最上位キーを数えたもの（`Language` を除いて166）。

**ポインタ・参照の `*` / `&` は型側に寄せる**（`char* p`, `const std::string& s`）。
`DerivePointerAlignment: false` + `PointerAlignment: Left` で固定しており、
ファイルごとの推定に任せていない。

なお **clang-format は識別子の名前には一切触れない**。命名は下記の `.clang-tidy` の担当。

### 命名 — `.clang-tidy` の `readability-identifier-naming`

由来は**書籍サンプルのスタイル**であって Google style ではない
（Google の private メンバは `snake_case_` で、`m_` プレフィックスは使わない）。
2026-07-30 に実コードの実態を読み取って文書化・機械化したもの。
唯一の例外は定数の `k` プレフィックスで、これは当時ルールが存在しなかったため
2026-08-13 に Google style から採った。

- クラス名 / 構造体名: PascalCase（`TriangleApp`, `FrameContext`）
- インターフェース: I プレフィックス（`ISampleApp`, `IBufferResource`）
- 関数: PascalCase（メンバー関数 `OnInitialize()`, 自由関数 `LoadShaderModule()`）
- メンバー変数: m\_ プレフィックス + camelCase（`m_vkInstance`）。
  ただし単純なデータ集約の struct のパブリックメンバは m\_ なし camelCase（`FrameContext::inflightFence`）
- 定数: k プレフィックス + PascalCase（`kPi`, `kAssetDirs`, `VulkanContext::kMaxInflightFrames`）。
  `constexpr` とクラスの `static const` が対象。ローカルの `const` は無検査
  （Cube 頂点の `A`..`H` のような数学記法を許すため）
- ローカル変数 / 引数: camelCase（`bufferSize`, `stackAngle`）
- グローバル変数（匿名 namespace 含む）: g\_ プレフィックス（`g_assetRoot`）
- 名前空間: lower_case（`loader`）

### その他

- `#pragma once` でヘッダーガード
- 推移的インクルードは行わず、ヘッダファイルは明示的に指定する

### 検査

`clang-tidy -p build/debug <file>`（詳細は `docs/notes/include-investigation.md`）。

ただし **`lib/stage1` 配下のヘッダは検査されていない**。`HeaderFilterRegex` が
サブディレクトリを含むパスにマッチしないため（章ディレクトリ直下のヘッダは対象）。
`lib` のヘッダに書いた名前が規約に従っているかは、今のところ人が見るしかない。
扱いは未決（`docs/notes/learning-log.md`）。

## Git 運用

### コミット

- **「変更の理由」ごとに分ける。** 大きさや作業時間ではなく理由が単位。
  判断基準は「単独で revert できるか」「`git blame` で読む人が納得するか」
- **整形と意味変更を混ぜない。** 混ぜざるを得ないときは、その旨をメッセージ本文に明記する
  （整形コミットは「読まなくてよい」と分かることに価値があるため）
- **今回の作業と無関係な変更を巻き込まない。** 特に依存関係の更新（`vcpkg.json` の
  baseline など）は単独コミットにする。混ぜると後から原因を追えなくなる
- メッセージは英語（既存履歴に合わせる）。**件名に「何をしたか」、本文に「なぜそうしたか」**。
  採らなかった選択肢とその理由も本文に残す

### ブランチとマージ

- 作業はブランチを切って行い、PR を経由して `main` に入れる
- **マージは `Create a merge commit`。** `Squash and merge` は使わない
  — コミットを分割した意図が失われるため
- マージ後はローカル・リモート双方のブランチを削除する
  （`git branch -d` / `git push origin --delete` / `git fetch --prune`）

## Vulkan 固有の注意

- GLM は `GLM_FORCE_DEPTH_ZERO_TO_ONE` と `GLM_FORCE_RADIANS` を定義すること
- エントリーポイントは `wWinMain`（Win32 アプリケーション）
- 実行ファイルは `add_executable(... WIN32 ...)` で作成
- ビルド後に DLL を自動コピーする POST_BUILD コマンドが各ターゲットに必要

## 章の追加方法

新しい章を追加するとき:

1. `XX_name/` ディレクトリを作成し、`main.cpp` と章固有のヘッダーを配置
2. `XX_name/CMakeLists.txt` を `01_triangle/CMakeLists.txt` をテンプレートとして作成
3. ルートの `CMakeLists.txt` に `add_subdirectory(XX_name)` を追加
