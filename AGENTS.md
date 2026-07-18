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

学習に関する会話を始めるときは、まず learning-log.md の最新エントリで現在地を把握すること。
写経がひと区切りついたら、ログへの追記をユーザに提案すること。

## ビルド

環境変数 `VCPKG_ROOT` が必要（vcpkg のルートパスを指す）。

```
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

- `.clang-format` 適用: Google style ベース、4スペースインデント、100カラム制限
- クラス名: PascalCase（`TriangleApp`）
- インターフェース: I プレフィックス（`ISampleApp`）
- メンバー関数: camelCase（`OnInitialize()`）
- メンバー変数: m\_ プレフィックス（`m_vkInstance`）
- `#pragma once` でヘッダーガード
- 推移的インクルードは行わず、ヘッダファイルは明示的に指定する

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
