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

## 2026-07-20

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
