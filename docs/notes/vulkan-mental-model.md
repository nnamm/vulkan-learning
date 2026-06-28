# Vulkan メンタルモデル ノート

> 「Vulkan実践入門」写経プロジェクトの学習ノート。
> 個々の `Vk*` API ではなく、**オブジェクト間の関係**と**時間軸上の順序**を掴むためのメモ。
> 習熟度: 三角形を描けた段階（1歩目）。目的は「何かを作る」より「Vulkanを使いこなして知る」こと。

---

## 使い方のコツ

- いきなり全部読むより、**各章の図を自分で再現 → このノートで答え合わせ**。
- 写経は2回やる。2回目は教科書を閉じ、**コメントを自分の言葉で先に書いてから**コードを埋める。
  書けない箇所が「今まだ霞んでいる点」。
- 効く絵は3枚: ①依存ツリー（空間） ②CPU/GPU 2本タイムライン（時間） ③1フレームのデータフロー。

---

## §1. `Vk*` を3種類に仕分ける

`Vk` で始まるものは役割が3つしかない。名前を見たら「モノ? 設計図? 動詞?」と機械的に分類する。

### ① ハンドル（モノ）
`VkInstance`, `VkDevice`, `VkBuffer`, `VkImage`, `VkPipeline`, `VkFence` ...
- GPU側に実体がある「不透明な参照」。中身は見えない（ポインタのような整数）。
- **必ず `vkCreate*`/`vkAllocate*` で生まれ、`vkDestroy*`/`vkFree*` で死ぬ**。生成と破棄が対。

### ② CreateInfo（設計図 = 注文票）
`VkBufferCreateInfo`, `VkSwapchainCreateInfoKHR` ...
- ハンドルを作るときに埋める構造体。全API共通の形:

```
VkXxxCreateInfo（設計図を埋める） → vkCreateXxx（GPUに作らせる） → VkXxx（ハンドルを得る）
```

- **コードの8割はこの「設計図を埋める作業」**。だから冗長に見える。

### ③ コマンド（動詞）
`vkCmdDraw`, `vkCmdBindPipeline`, `vkCmdBeginRendering` ...
- `vkCmd` で始まる = **コマンドバッファに「録画」される命令**。即実行ではない。
- `Begin()`〜`End()` の間に記録され、`vkQueueSubmit` で初めてGPUに送られ実行。
- `vkCmd` が付かない関数（`vkCreate*` 等）はCPU側で即実行。
- **`vkCmd` の有無の区別が決定的に重要。**

---

## §2. オブジェクトの依存ツリー

Vulkanのオブジェクトは厳密なツリー。`VulkanContext::Initialize`（`lib/stage1/core/vulkan_context.cpp:46`）の初期化順序と一致する。

```
VkInstance                        ← アプリとVulkanの接点
 └ VkPhysicalDevice               ← 物理GPU（選ぶだけ。作らない）
     └ VkDevice                   ← 論理デバイス。以降ほぼ全APIの第1引数
         ├ VkQueue                ← コマンドの投入口
         ├ VkCommandPool → VkCommandBuffer
         ├ VkDescriptorPool → VkDescriptorSet
         ├ VkSwapchainKHR → VkImage / VkImageView
         ├ VkBuffer / VkImage (+ VkDeviceMemory)
         ├ VkPipeline
         └ VkFence / VkSemaphore  ← 同期プリミティブ
```

含意:
- **破棄は生成の逆順**。`Cleanup()`（`vulkan_context.cpp:54`）は Device → Instance の順。
  子が生きてるのに親を壊すとクラッシュ。
- ほぼ全ての `vkCreate*` の第1引数が `VkDevice` = **論理デバイスが全リソースの所有者**。
  `VkDevice` を中心に放射状にぶら下がる絵を描くと繋がる。

---

## §3. フェンスとセマフォ：「誰が誰を待つか」

違いは1点だけ: **待つ主体がCPUかGPUか**。

|                  | Fence                          | Semaphore                       |
|------------------|--------------------------------|---------------------------------|
| 待つ主体          | **CPU**（ホスト）               | **GPU**（キュー間）              |
| 用途             | GPUの仕事が終わったかCPUが確認    | GPU処理Aの完了を処理Bが待つ       |
| CPUから状態が見える | はい（`vkWaitForFences`）        | いいえ（CPUは触れない）           |
| 典型例           | フレーム完了待ち                 | acquire→描画→present の連結       |

### Fence（CPU↔GPU） — `vulkan_context.cpp:103` AcquireNextImage

```cpp
vkWaitForFences(..., &fence, VK_TRUE, UINT64_MAX);  // CPUがここで止まって待つ
...
vkResetFences(..., &fence);                          // 待ち終えたらリセット
```

投入時にGPUへ「終わったら立てろ」と渡す（`:142`）:

```cpp
vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, frame.inflightFence);
//                                              ↑完了したらGPUがsignalする
```

→ 「2フレーム前のコマンドバッファ、もう再利用していい?」をCPUが確認する仕組み。
   これがないと、まだ描画中のコマンドバッファを上書きしてしまう。

### Semaphore（GPU↔GPU） — `vulkan_context.cpp:123` SubmitPresent

```cpp
submitInfo.pWaitSemaphores   = &presentCompleteSem;  // 画像取得が済むまで描画開始するな
submitInfo.pSignalSemaphores = &renderCompliteSem;   // 描画が終わったら立てる
vkQueueSubmit(...);
m_swapchain->QueuePresent(...);  // present側はrenderCompleteを待つ（swapchain.cpp:154）
```

連鎖:

```
[acquire] --presentComplete--> [描画 draw] --renderComplete--> [present 表示]
```

CPUはこの待ち合わせに一切関与しない。GPUが内部で順序を守る。

### 落とし穴2つ
- **初期SIGNALED**（`vulkan_context.cpp:363`）: フェンスは `VK_FENCE_CREATE_SIGNALED_BIT` で
  「最初から立った状態」で作る。1フレーム目の `vkWaitForFences` で永久に止まらないため。
- **2系統が併存する理由**: Fenceは「コマンドバッファ再利用の安全性（リソース寿命）」、
  Semaphoreは「描画パイプライン段階の順序（実行順）」。**守る対象が違う**ので両方要る。

### 描くべき図
CPU軸とGPU軸の2本タイムライン。Fence = CPUが横切る縦線、Semaphore = GPU内の横矢印。
→ `AcquireNextImage` → `SubmitPresent` の実際の関数呼び出しに対応づける。**（次回の宿題）**

---

## §4. これらを支えるC++の考え方

要は「`Vk*` ハンドルという生資源を、C++のRAIIでどう飼い慣らすか」。

### (a) ハンドルは生ポインタと同じ＝危険物
中身が見えない整数で、デストラクタも参照カウントもない。作ったら手で壊す必要 = 生 `new`/`delete` と同じ危うさ。

### (b) ライブラリの答え: RAIIで包む
`CommandBuffer`（`lib/stage1/core/command_buffer.h:5`）が例。生 `VkCommandBuffer` をメンバに持ち、
`operator VkCommandBuffer()`（`:16`）で必要時は生ハンドルとして振る舞う。
API に渡すときは暗黙変換、管理はC++オブジェクトに任せる。

### (c) shared_ptr + コピー禁止 = GPU資源の所有権モデル
`gpu_resource_base.h`:

```cpp
GpuResourceBase(const GpuResourceBase&) = delete;   // コピーすると二重破棄 → 禁止
static std::shared_ptr<T> Create() { ... }          // 必ず参照カウント管理
```

GPU資源は「一つの実体を複数で参照、最後の1人が消えたら破棄」が自然 = `shared_ptr` + コピー禁止。
**C++のRAII/所有権の道具を、寿命を持つGPUハンドルに当てはめている** ——ここが核。

> 注: Fence/Semaphore 自体はこのライブラリではまだ生ハンドル（`VkFence inflightFence`,
> `vulkan_context.h:52`）で手動管理。`CommandBuffer` のようなRAIIラッパーにはしていない。
> 「もし自分でRAIIラッパーを書くなら?」と考えるのが (b)(c) の理解確認の良い練習。

---

## §5. 学習法（メタ）

Vulkanの難しさは個々のAPIでなく**オブジェクト間の関係と時間軸上の順序**にある。
コードは関係を平たくしか表現できない。だから:

- **読む** = 個々のAPIの意味を知る（点を作る）
- **書く（写経）** = 手が順序を覚える（点を強くする）
- **描く** = 関係と時間軸を可視化する（**点を線にする = シナプス**）← ここの比重を上げると効率が跳ねる

三角形が描けた時点で **同期・リソース・パイプラインの3大概念に全部触れている**。
主要な点は既に持っている。あとは繋ぐ作業 = 地道な「描く」が最短路。

---

## 読書順 / 次にやること

1. `01_triangle/main.cpp` — 骨組みと初期化順序
2. `vulkan_context.cpp:43` Initialize — Vulkan初期化の定石
3. `triangle_app.cpp:129` OnDrawFrame + `vulkan_context.cpp:103/123` — **同期とフレーム（最重要・最難関）**
4. `gpu_resource_base.h` → `buffer_resource.h` — リソース設計のイディオム

### 次回の宿題（未完）
- [ ] §3 の CPU/GPU 2本タイムライン図を、`AcquireNextImage`→`SubmitPresent` の実際の呼び出しに対応づけて描く
- [ ] §4 の「Fence/Semaphore を自分でRAIIラッパー化するなら?」を考える
