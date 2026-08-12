# lib/stage1 ファイルマップ

> `lib/stage1` の各ファイルが「何のためにあり・何をして・どこまで責任を持ち・誰が使うか」を
> ファイル単位で引くための索引。
> 対象コミット時点: 4章（`02_simplecube`）まで写経完了。ソースを直接読んで記述しており、推測は含めない。
> 未使用・未実装が確認できたものは §7 に事実として分離した。
>
> `vulkan-mental-model.md` との役割分担:
>
> - **メンタルモデル** = 概念から入る（同期とは何か → 該当コードはここ）
> - **このファイル** = コードから入る（`swapchain.cpp` とは何者か → 概念はここ）

---

## §1. 全体像：5つのレイヤ

`lib/stage1` は 25 ファイル（ヘッダ 14 / 実装 11）。役割で並べるとこうなる。

```text
[L5] アプリ骨格   common/sample_app.h            ISampleApp（3つの仮想関数）
                        ▲ 実装
                  01_triangle/triangle_app, 02_simplecube/simplecube_app
                        │ 下の L4〜L0 を使う
                        ▼
[L4] リソース     gpu_resource_base.h ──(CRTP基底)──┬─ buffer_resource
                                                    └─ image_resource
                  resource_uploader  ──(使う)──> buffer_resource / command_buffer

[L3] 道具         command_buffer ──(使う)──> image_barrier
                  graphics_pipeline_builder
                  shader_loader

[L2] 中核         vulkan_context  ⇄  swapchain      ← ここが唯一の「状態の持ち主」

[L1] 境界         surface_provider.h（I） ◀── glfw_surface_provider（GLFW実装）

[L0] 非Vulkan     asset_path                        ← vulkan.h を一切見ない
```

**依存の集中点は `VulkanContext::Get()`。** L3・L4 のほぼ全ファイルが、コンストラクタ引数で
`VkDevice` を受け取るのではなく、必要になった場所でシングルトンから取りに行く。
「所有はしていないが参照はする」構造なので、ファイル単体を読むときは
`VulkanContext::Get()` の呼び出し箇所＝外部依存の入口として拾うとよい。

---

## §2. 一覧（逆引き表）

| ファイル | 一言でいうと | 主な利用者 |
| --- | --- | --- |
| `common/sample_app.h` | 章アプリが実装する3メソッドの型 | 各章の `*_app.h` |
| `core/asset_path.{h,cpp}` | 実行ファイル基準のアセットパス解決 | `main.cpp`, 各 `*_app.cpp` |
| `core/surface_provider.h` | 「ウィンドウ」の抽象。VulkanContext を GLFW から切る | `VulkanContext` |
| `core/glfw_surface_provider.{h,cpp}` | 上の GLFW 実装 | `main.cpp` |
| `core/vulkan_context.{h,cpp}` | Instance〜Pool〜フレーム同期。全体の司令塔 | 全員 |
| `core/swapchain.{h,cpp}` | 表示先イメージ列とセマフォの管理 | `VulkanContext`, 各アプリ |
| `core/gpu_resource_base.h` | GPU資源を `shared_ptr` 生成に強制する CRTP 基底 | `BufferResource`, `ImageResource` |
| `core/buffer_resource.{h,cpp}` | 4種のバッファ（Vertex/Index/Uniform/Staging） | 各アプリ, `ResourceUploader` |
| `core/image_resource.{h,cpp}` | 3種のイメージ（Depth/Texture2D/StorageImage2D） | `SimpleCubeApp`（Depthのみ） |
| `core/image_barrier.{h,cpp}` | レイアウト遷移パラメータの名前付きプリセット | `CommandBuffer`, 各アプリ |
| `core/command_buffer.{h,cpp}` | `VkCommandBuffer` の RAII 包み＋遷移ヘルパ | `VulkanContext`, `ResourceUploader`, 各アプリ |
| `core/resource_uploader.{h,cpp}` | ステージング転送を溜めて一括実行 | `SimpleCubeApp` |
| `core/shader_loader.{h,cpp}` | `.spv` を読んで `VkShaderModule` 化 | 各アプリ |
| `core/graphics_pipeline_builder.{h,cpp}` | `VkGraphicsPipelineCreateInfo` の組み立て | 各アプリ |

ビルド対象は各章の `CMakeLists.txt` が個別に列挙する（`lib` 側に `CMakeLists.txt` は無い）。
`01_triangle` は 9 本、`02_simplecube` は 11 本を指定しており、差分は
`image_resource.cpp` と `resource_uploader.cpp` の2本＝**4章で増えた分**。

---

## §3. 中核（L2）

### `core/vulkan_context.{h,cpp}` — 司令塔

**目的**: アプリ全体で1つしか無いもの（Instance / PhysicalDevice / Device / Queue /
CommandPool / DescriptorPool / フレーム同期）を1箇所に集める。

**シングルトン**: `Get()` が関数ローカル static を返す。コンストラクタ・デストラクタは private
なので、外部から生成も破棄もできない。生存期間はプロセス寿命で、片付けは `Cleanup()` を明示的に呼ぶ。

**`Initialize()` の順序**（`vulkan_context.cpp:40`）:

```text
CreateInstance → PickPhysicalDevice → CreateDebugMessenger → CreateLogicalDevice
              → CreateCommandPool → CreateDescriptorPool
```

`CreateSurface()` はここに**含まれない**。`RecreateSwapchain()` の中で
`m_surface == VK_NULL_HANDLE` のときだけ呼ばれる（`vulkan_context.cpp:86`）。
`main.cpp` が `Initialize()` の直後に `RecreateSwapchain()` を呼ぶことで初めてサーフェスができる。

**各段階の中身**:

| 関数 | していること |
| --- | --- |
| `CreateInstance` | `apiVersion = 1.3`。Debug ビルドのみ `VK_EXT_debug_utils` と `VK_LAYER_KHRONOS_validation` を追加 |
| | ウィンドウ系拡張は `GetWindowSystemExtensions` コールバック（`main.cpp` が GLFW から詰める）で注入 |
| `PickPhysicalDevice` | `devices[0]` 決め打ち。メモリプロパティとデバイスプロパティを取得しておく |
| `CreateLogicalDevice` | `VK_QUEUE_GRAPHICS_BIT` を持つ最初のファミリを選択。拡張は swapchain と maintenance1 |
| `BuildVkFeatures` | `pNext` チェーンを可変長テンプレートで連結 → サポート照会 → `dynamicRendering` と `synchronization2` を有効化 |
| `CreateSurface` | Provider に作らせ、そのファミリが Present 可能か検査。不可なら例外 |
| `CreateCommandPool` | `RESET_COMMAND_BUFFER_BIT` 付き |
| `CreateDescriptorPool` | 5種×4096、`maxSets = 4096`、`FREE_DESCRIPTOR_SET_BIT` 付き |

**フレーム同期の責務**（メンタルモデル §3 / §3.5 の実体）:

- `MaxInflightFrames = 2`。`FrameContext { commandBuffer, inflightFence }` を2つ持つ
- Fence は `VK_FENCE_CREATE_SIGNALED_BIT` で作る（1周目で待ちが即通るように）
- `AcquireNextImage()` = Fence 待ち → `Swapchain::AcquireNextImage()` → 成功時のみ Fence をリセット。
  `VK_ERROR_OUT_OF_DATE_KHR` なら幅・高さが 0 でないときに限り `Swapchain::Recreate()`（最小化対策）
- `SubmitPresent()` = `presentComplete` を待ち `renderComplete` を signal、
  `inflightFence` を添えて `vkQueueSubmit` → `Swapchain::QueuePresent()` → `AdvanceFrame()`。
  待機ステージは `COLOR_ATTACHMENT_OUTPUT_BIT`

**Cleanup の順序**: `vkDeviceWaitIdle` → FrameContexts → CommandPool → DebugMessenger →
Swapchain → Surface → Device → Instance。**生成の逆順**。

**その他の提供物**: `FindMemoryType()`（要件ビットとプロパティの両方を満たす型を線形探索）、
`CreateCommandBuffer()`（PRIMARY を1本、`shared_ptr<CommandBuffer>` で返す）、
`AllocateDescriptorSet()` / `FreeDescriptorSet()`、`SetDebugObjectName()`（Debug ビルドのみ動作）。

**責務の境界**: 「1つしか無いもの」と「フレームの進行」を持つ。
逆に、**何をどう描くかは一切持たない**。パイプラインもリソースもアプリ側の所有物。

---

### `core/swapchain.{h,cpp}` — 表示先とセマフォ

**目的**: 表示先イメージの列と、それに紐づく同期オブジェクトを管理する。

**`Recreate()` がしていること**:

1. `VkSurfaceCapabilitiesKHR` を取得。`currentExtent.width == UINT32_MAX` のときだけ引数の幅高を使う
2. フォーマット選択 — `SRGB_NONLINEAR` かつ `B8G8R8A8_UNORM` / `R8G8B8A8_UNORM` を優先、無ければ `formats[0]`
3. `minImageCount = caps.minImageCount + 1` / `presentMode = FIFO` /
   `usage = COLOR_ATTACHMENT | TRANSFER_DST` / `oldSwapchain = 現ハンドル`
4. `vkDeviceWaitIdle` してから `vkCreateSwapchainKHR`
5. イメージ取得 → 各 `VkImageView` を作成 → `CreateFrameContext()`

**セマフォの持ち方がこのファイルの肝**（メンタルモデル §3.5「presentComplete はプール方式」の実体）:

| 種類 | 個数 | 持ち方 |
| --- | --- | --- |
| `renderComplete` | イメージ数 | `m_frames[i]` に固定で紐づく |
| `presentComplete` | イメージ数 + 1 | `m_presentSemaphoreList` にプールし、Acquire のたびに貸し借りする |

`AcquireNextImage()` はプールから1本 `pop_back` して `vkAcquireNextImageKHR` に渡す。
失敗したら即座に返却。成功したら、そのインデックスに前回紐づいていたセマフォをプールへ戻し、
今回借りたものを `m_frames[index].presentComplete` に差し替える。
**どのイメージが返るか事前に分からない**以上、イメージ数で固定紐づけできないための構造。

**`friend class VulkanContext`**: private な `CreateFrameContext` / `DestroyFrameContext` を
`VulkanContext` から触れるようにしているが、現状の呼び出しは `Swapchain` 内部で完結している。

---

## §4. リソース層（L4）

### `core/gpu_resource_base.h` — 生成の作法を強制する

17行しかないが設計の要。CRTP（`GpuResourceBase<T>` を `T` が継承する）で以下を強制する。

- **コピー禁止**（コピーコンストラクタ・代入を `delete`）— GPU ハンドルの二重解放を型で防ぐ
- **`protected` コンストラクタ + `static Create()`** — スタック生成を封じ、必ず `shared_ptr<T>` で持たせる

「GPU 資源は生ポインタと同じ危険物なので、所有権を型で縛る」というライブラリ側の答え
（メンタルモデル §5）。派生の `BufferResource<T>` / `ImageResource<T>` は
`Create()` を隠蔽し、`Create(size, ...)` のように **生成と初期化を1回で済ませる版**を各具象クラスに置いている。

---

### `core/buffer_resource.{h,cpp}` — 4種のバッファ

**構造**: `IBufferResource`（純粋仮想インターフェース）と
`BufferResource<T> : GpuResourceBase<T>, IBufferResource`（共通実装）の2段。
インターフェースが分かれている理由は `ResourceUploader` が **具象型を知らずに** 転送先を扱うため。

**`BufferResource<T>` が持つ共通処理**:

- `CreateBuffer()` — `vkCreateBuffer` → メモリ要件取得 → `FindMemoryType` → `vkAllocateMemory` → `vkBindBufferMemory`
- `Cleanup()` — バッファとメモリを破棄。デストラクタからも呼ぶ
- `GetDescriptorInfo()` — `VkDescriptorBufferInfo{buffer, 0, size}` を返す
- `m_accessFlags` — このバッファが「次にどうアクセスされるか」を CPU 側で覚えておく箱。バリア生成時に使う

**具象4種の違いは `Initialize()` の3点だけ**:

| クラス | usage | メモリプロパティ | AccessFlags |
| --- | --- | --- | --- |
| `VertexBuffer` | `VERTEX_BUFFER \| TRANSFER_DST` | **引数で指定** | `VERTEX_ATTRIBUTE_READ` |
| `IndexBuffer` | `INDEX_BUFFER \| TRANSFER_DST` | **引数で指定** | `INDEX_READ` |
| `UniformBuffer` | `UNIFORM_BUFFER` | `HOST_VISIBLE \| HOST_COHERENT` 固定 | `SHADER_READ` |
| `StagingBuffer` | `TRANSFER_SRC` | `HOST_VISIBLE \| HOST_COHERENT` 固定 | `HOST_WRITE` |

`VertexBuffer` / `IndexBuffer` だけメモリプロパティが引数なのは、**同じ型を2通りに使うため**。
`TriangleApp` は `HOST_VISIBLE` で作って直接 `Map()` し、`SimpleCubeApp` は `DEVICE_LOCAL` で作って
`ResourceUploader` 経由で転送する。この2通りに対応して、両者の `Map()` / `Unmap()` は
`HOST_VISIBLE` でなければ `nullptr` を返す／何もしない実装になっている
（`UniformBuffer` / `StagingBuffer` は常に `HOST_VISIBLE` なので無条件に実行する）。

**なぜ明示的インスタンス化が無くてもリンクが通るのか**: `buffer_resource.cpp` 末尾の
`template class BufferResource<VertexBuffer>;` はコメントアウトされている（`image_resource.cpp` では有効）。
それでも通るのは、`BufferResource<T>::Cleanup()` が**仮想関数**だから。
この .cpp で `VertexBuffer::Initialize()` などを定義する際に基底クラスが暗黙的にインスタンス化され、
vtable を作るために仮想関数の実体も一緒に生成される。実際、
`nm -C buffer_resource.cpp.o` では `W BufferResource<VertexBuffer>::Cleanup()`（実体あり）、
`nm -C triangle_app.cpp.o` では `U`（未定義）となり、前者が後者を埋めている
（`learning-log.md` 2026-07-17 の「vtable for X が undefined」の話と同じ仕組み）。

---

### `core/image_resource.{h,cpp}` — 3種のイメージ

構造は buffer 側と同じ2段（`IImageResource` + `ImageResource<T>`）。バッファとの決定的な違いは
**レイアウト（`m_layout`）を CPU 側で覚えておく必要がある**こと。イメージには「今どの状態か」があり、
Vulkan は問い合わせる手段を提供しないので、自分で追跡するしかない。
`SetLayout()` / `GetLayout()` / `SetAccessFlag()` / `GetAccessFlags()` はそのための口で、
実際に更新するのは `CommandBuffer::TransitionLayout()` のテンプレート版。

各具象クラスは `vkCreateImage` → メモリ確保・バインド → `VkImageView` 作成までを一度に行う。
違いは usage と aspect:

| クラス | usage | aspect | 用途 |
| --- | --- | --- | --- |
| `DepthBuffer` | `DEPTH_STENCIL_ATTACHMENT` | `DEPTH` | 深度バッファ。`SimpleCubeApp` が `D32_SFLOAT` で使用 |
| `Texture2D` | `SAMPLED \| TRANSFER_SRC \| TRANSFER_DST` | `COLOR` | テクスチャ（4章時点では未使用） |
| `StorageImage2D` | 上記 + `STORAGE` | `COLOR` | シェーダから読み書きする画像（4章時点では未使用） |

メモリはいずれも `DEVICE_LOCAL`。ファイル末尾に3つの明示的インスタンス化がある
（`template class ImageResource<DepthBuffer>;` など）。

---

### `core/resource_uploader.{h,cpp}` — ステージング転送のまとめ役

**目的**: `DEVICE_LOCAL`（CPU から書けない）バッファへのデータ投入を、
**溜めておいて一括で1回だけ流す**。

**3つのメソッドの役割**:

| メソッド | 中身 |
| --- | --- |
| `Initialize()` | 使い回し用の `VkFence` を1本作る（unsignaled） |
| `UploadBuffer()` | 転送先が `IsHostAccessible()` なら**その場で `memcpy` して即 return**。そうでなければ `StagingBuffer` を作って `memcpy` し、積むだけ |
| `SubmitAndWait()` | 溜めた分をまとめて実行 |

`SubmitAndWait()` の順序が読みどころ:

1. コマンドバッファを1本確保し `ONE_TIME_SUBMIT` で開始
2. 全エントリの `vkCmdCopyBuffer` を**先に全部記録**
3. `VkBufferMemoryBarrier2` を全エントリ分作り、**`vkCmdPipelineBarrier2` は1回だけ**発行
   （`TRANSFER_WRITE` → 各バッファの `GetAccessFlags()`）
4. submit → `vkWaitForFences` で完了待ち → エントリをクリアしコマンドバッファを解放

コピーとバリアを分けて「バリア1回」にまとめるのがこのクラスの存在意義。
**同期実行**（呼ぶと完了まで戻らない）なので、初期化時にのみ使う想定。

イメージ転送（`UploadImage` / `CreateMipmap` / `PendingImageTransfer`）は
コメントアウトで枠だけ残っており、5章以降で埋まる場所。

---

## §5. 道具（L3）

### `core/command_buffer.{h,cpp}`

`VkCommandBuffer` の RAII ラッパー。生成は `VulkanContext::CreateCommandBuffer()` に一元化され、
デストラクタで `vkFreeCommandBuffers` する（`shared_ptr` の参照が切れた時点で解放）。

- `Begin()` / `End()` / `Reset()` — 生 API の薄い包み
- `operator VkCommandBuffer()` — 暗黙変換。`vkCmdBindPipeline(*commandBuffer, ...)` と書けるのはこれのおかげ
- `TransitionLayout(VkImage, range, transition)` — `VkImageMemoryBarrier2` を組んで `vkCmdPipelineBarrier2` を1回
- `TransitionLayout(shared_ptr<T>, transition)`（テンプレート版）— 上を呼んだうえで、
  `image->SetAccessFlag()` / `SetLayout()` で**CPU 側の追跡状態も更新**する

テンプレート版は `T` に `GetVkImage()` と `GetSubresourceRange()` を要求する。
4章時点の各アプリは、スワップチェインイメージ（`VkImage` 生ハンドル）を扱うので
**非テンプレート版のみを使っている**。

なお `command_buffer.h` と `vulkan_context.h` は相互に `#include` しあっているが、
両者とも相手方を前方宣言しているため `#pragma once` で解決している。

### `core/image_barrier.{h,cpp}`

`ImageLayoutTransition` は6フィールド（old/new レイアウト、src/dst アクセスマスク、src/dst ステージ）の
単なるデータ集約。価値は**名前付きの静的ファクトリ**にあり、
「どのレイアウトからどこへ、どのステージで、どのアクセスを待つか」の正しい組み合わせを7通り固定している。

| ファクトリ | 4章時点の使用 |
| --- | --- |
| `FromUndefinedToColorAttachment()` | 各アプリの `OnDrawFrame` 冒頭で使用 |
| `FromColorToPresent()` | 各アプリの `OnDrawFrame` 末尾で使用 |
| `FromPresentSrcToColorAttachment()` | 未使用 |
| `FromUndefToTransferDst()` / `FromTransferDstToTransferSrc()` | 未使用（ミップマップ生成向け） |
| `ToShaderReadonlyOptimal()` / `ToStorageImageGeneralLayout()` | 未使用。`IImageResource*` から現在のレイアウトを読んで `oldLayout` に入れる |

`FromUndefinedToColorAttachment()` を毎フレーム使えるのは、
アタッチメントを `LOAD_OP_CLEAR` で開始しており前の内容を保持する必要がないため
（各アプリのコメントにも明記されている）。

### `core/shader_loader.{h,cpp}`

`namespace loader` の自由関数1本。`.spv` を `ate | binary` で開いてサイズを取り、
読み込んで `vkCreateShaderModule` する。失敗は例外（`std::runtime_error`）。
**破棄はしない** — 返された `VkShaderModule` は呼び出し側が
パイプライン作成後に `vkDestroyShaderModule` する（両アプリともそうしている）。

### `core/graphics_pipeline_builder.{h,cpp}`

**目的**: `VkGraphicsPipelineCreateInfo` の巨大な設定群を、必要な項目だけ触れば済む形にする。

コンストラクタで既定値を敷く: `TRIANGLE_LIST` / `POLYGON_MODE_FILL` / `CULL_MODE_NONE` /
`FRONT_FACE_COUNTER_CLOCKWISE` / 1サンプル / ブレンド無効 / **深度テスト無効**。
`SimpleCubeApp` が `SetDepthStencilState()` と `SetRasterizationState()` で
深度テストと背面カリングを上書きするのは、この既定値を打ち消すため。

**`SetViewport()` の2つのオーバーロードは挙動が違う** — 読み違えやすい箇所:

| 呼び方 | 挙動 |
| --- | --- |
| `SetViewport(VkExtent2D)` | `y = height`, `height = -height` に書き換え、`VK_KHR_maintenance1` の負ビューポートで**上下反転する** |
| `SetViewport(VkViewport&, VkRect2D)` | 渡された値をそのまま使う（**反転しない**） |

`TriangleApp` は後者、`SimpleCubeApp` は前者を使っている。

`Build()` は `UseRenderPass()` / `UseDynamicRendering()` のどちらを呼んだかで分岐し、
後者なら `VkPipelineRenderingCreateInfo` を `pNext` に繋いで `renderPass = VK_NULL_HANDLE` にする。
両アプリとも Dynamic Rendering 側。失敗時は例外ではなく `VK_NULL_HANDLE` を返す。

戻り値の型が揃っていない点に注意: 多くは `GraphicsPipelineBuilder&` を返してチェーンできるが、
`SetColorBlendAttachmentState` / `SetRasterizationState` / `SetDepthStencilState` は `void`。

---

## §6. 境界と骨格（L0・L1・L5）

### `core/surface_provider.h` / `core/glfw_surface_provider.{h,cpp}`

`ISurfaceProvider` は3メソッド（`CreateSurface` / `GetFramebufferWidth` / `GetFramebufferHeight`）だけの
インターフェース。**これがあるおかげで `vulkan_context.cpp` は GLFW を一切 include しない。**
`VulkanContext` がウィンドウに用があるのは「サーフェスを作る時」と
「再生成のためにサイズを聞く時」の2場面しかない、という設計判断がそのまま型になっている。

ただしインスタンス拡張名の取得だけはこのインターフェースに載っておらず、
`VulkanContext::GetWindowSystemExtensions`（`std::function` の public メンバ）に
`main.cpp` がラムダを差し込む形になっている。**ウィンドウ依存の入口は2つある**、と覚えておく。

`GLFWSurfaceProvider` は `glfwCreateWindowSurface` と `glfwGetFramebufferSize` を呼ぶだけの薄い実装。
`GLFWwindow*` は借り物で、破棄は `main.cpp` の責任。

### `core/asset_path.{h,cpp}`

`lib/stage1` で唯一 `vulkan.h` を include しないファイル。

- 匿名 namespace の `g_assetRoot`（既定 `"assets/"`）を保持
- `SetAssetRootPath()` は `absolute()` → `canonical()` で正規化する。
  `main.cpp` が実行ファイルの位置から `exeDir / "../../../assets"` を渡している
- `GetAssetPath(type, fileName)` = ルート / 種別サブディレクトリ / ファイル名。
  サブディレクトリ名は `AssetType` と1対1の `constexpr std::array`（`shaders` / `textures` / `models`）

4章時点で使われているのは `AssetType::Shader` のみ。

### `common/sample_app.h`

`ISampleApp` = `OnInitialize()` / `OnDrawFrame()` / `OnCleanup()` の3つの純粋仮想関数だけ。
`TriangleApp` と `SimpleCubeApp` が実装する。

末尾に `GLM_FORCE_DEPTH_ZERO_TO_ONE` 未定義なら `#error` を出すチェックがある。
GLM の既定は OpenGL 流の深度範囲 −1〜1 だが Vulkan は 0〜1 なので、
定義忘れを**コンパイル時に**止める仕掛け。

なお両章の `main.cpp` は `TriangleApp theApp{};` のように**具象型で直接持っており**、
`ISampleApp*` 経由の多態呼び出しはしていない。現状このインターフェースは
「章アプリが実装すべき形」を示す規約として機能している。

---

## §7. 4章時点で未使用・未接続と確認できたもの

読解の邪魔になるので分離した。いずれもソースを grep して確認した事実。

### 宣言のみで定義が存在しない

- `VulkanContext::SubmitAndWait(std::shared_ptr<CommandBuffer>)` — `vulkan_context.h:63` に宣言があるが
  実装がどこにも無い。呼び出しも無いためリンクは通る。
  同名の `ResourceUploader::SubmitAndWait()` は別物（そちらは実装・使用ともにある）

### 代入されないメンバ

- `VulkanContext::m_presentQueueFamilyIndex` — `GetPresentFamily()` が返すが値は一度も設定されない。
  Present 可否は `CreateSurface()` がグラフィックスキューについて検査し、不可なら例外を投げる方針
- `VulkanContext::m_atomicFloatFeatures` — 宣言のみ。`BuildVkFeatures()` の `pNext` チェーンに繋がれていない
- `GraphicsPipelineBuilder::m_device` — 宣言のみ。`Build()` は `VulkanContext::Get().GetVkDevice()` を使う

### 未使用の API

- `Swapchain::GetImageViews()` / `GetImageCount()` / `GetCurrentIndex()` / `operator const VkSwapchainKHR()`
- `CommandBuffer::Reset()`、`IBufferResource::GetDescriptorInfo()`
- `ImageLayoutTransition` の5ファクトリ（§5 の表を参照）
- `GraphicsPipelineBuilder::UseRenderPass()` / `SetInputAssembly()` / `SetTessellation()` /
  `SetColorBlendAttachmentState()`
- `Texture2D` / `StorageImage2D`、`AssetType::Texture` / `AssetType::Model`

### 気づいた実装上の齟齬（要確認）

- `VulkanContext::AllocateDescriptorSet()`（`vulkan_context.cpp:403`）— `VkDescriptorSetAllocateInfo` の
  `sType` に `VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO` を設定している。
  正しくは `VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO`
- `ResourceUploader::SubmitAndWait()` — バリアの `dstAccessMask` に
  `UploadBuffer()` の引数 `nextAccessMask`（`entry.dstAccessMask` に保存済み）ではなく
  `dst->GetAccessFlags()` を使っている。現状は両者が一致するため結果は変わらない
- `Swapchain::Recreate()` を2回目以降に呼ぶと、`m_imageViews` を `clear()` せずに `push_back` し、
  `oldSwapchain` に渡した旧スワップチェインを破棄せず、`DestroyFrameContext()` も呼ばずに
  `CreateFrameContext()` する。現状は起動時1回と `OUT_OF_DATE` 時のみの呼び出しで、
  かつ両章とも `GLFW_RESIZABLE` を `FALSE` にしているため表面化していない

---

## §8. 読む順のおすすめ

Vulkan の基礎を復習する目的なら、依存の少ない順ではなく**1フレームが流れる順**に読むのが早い。

1. `main.cpp`（各章）— 誰が誰を初期化するか。ここが全体の目次
2. `vulkan_context.cpp` の `Initialize` 系 — 静的な骨格（メンタルモデル §2）
3. `swapchain.cpp` `Recreate` / `CreateFrameContext` — 表示先とセマフォの用意
4. `vulkan_context.cpp` の `AcquireNextImage` / `SubmitPresent` + `swapchain.cpp` の
   `AcquireNextImage` / `QueuePresent` — **同期の本丸**（メンタルモデル §3, §3.5）
5. `gpu_resource_base.h` → `buffer_resource.h` → `image_resource.h` — 所有権のイディオム（メンタルモデル §5）
6. `resource_uploader.cpp` — ステージング転送（メンタルモデル §4.1）
7. `graphics_pipeline_builder.cpp` `Build` — パイプラインに何が必要かの一覧として読む
8. 各章の `*_app.cpp` の `OnDrawFrame` — 1〜7 がどう組み上がるかの答え合わせ
