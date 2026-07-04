# 学習ログ

> 写経の進捗・現在地・次にやることを記録するログ。Windows / Kubuntu の2環境をまたいで
> 学習コンテキストを引き継ぐための「共有メモリ」。AIセッション開始時はまず最新エントリを読むこと。
>
> 運用ルール:
> - 新しいエントリを**上に**追加する（日付は `YYYY-MM-DD`）
> - 各エントリは「やったこと / 現在地 / 次にやること / 気づき」の4点セット
> - 写経がひと区切りついたタイミングで追記する（毎日でなくてよい）

---

## 2026-07-04

### やったこと
- `docs/notes/vulkan-mental-model.md` の位置づけを再確認 —「写経とAIを通じた学びの道しるべ」
- 学習コンテキストを2環境（Windows / Kubuntu）で共有するため、この学習ログを新設

### 現在地: 4章 `02_simplecube` 写経中（章4.9まで完了）

**02_simplecube の実装状況:**

| 済 | 内容 |
|----|------|
| ✅ | `CreateCubeGeometry` — 24頂点+36インデックス、ResourceUploader 経由で DEVICE_LOCAL に転送 |
| ✅ | `CreateDescriptorSetLayout` — UBO 1個 (binding=0, VS+FS) |
| ✅ | `CreateUniformBuffers` — インフライトフレーム数(2)分の `UniformBuffer` |
| ✅ | `CreateDescriptorSets` — `AllocateDescriptorSet` + `vkUpdateDescriptorSets` |
| ✅ | `CreateDepthBuffer` — `D32_SFLOAT` |
| 🔶 | `CreateGraphicsPipeline` — PipelineLayout まで。**VkPipeline 本体は未着手** |
| ⬜ | `OnDrawFrame` / `OnCleanup` — 未実装 |
| ⬜ | `main.cpp` — 空ファイル（エントリーポイント未記述） |
| ⬜ | シェーダーコンパイル — `cube.vert` / `cube.frag` は記述済みだが `.spv` 未生成 |

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
