# 源码分层架构

本页按源码中的实际职责归纳，而不是按目录名。中文版与英文版同步：[Source Layer Architecture](../../en/guide/architecture.md)；术语定义见[术语表](glossary.md)。

## 抽象层模型

| 层 | 职责 |
|---|---|
| L1 协议与身份 | 定义可移植消息字段、ActorId 布局和可替换身份编码规则。 |
| L2 确定性 Epoch 处理 | 排序输入批次、折叠 payload，并输出每个 Epoch 的状态与哈希。 |
| L3 传输边界与本地队列 | 提供 `send`/`poll`/`close`，并保持本地消息所有权显式。 |
| L4 Aeron 投递适配器 | 把传输边界桥接到 Publication/Subscription 生命周期与帧编解码。 |
| L5 原生 Aeron 网关 | 将 Aeron C 客户端状态封装在一个小型 C ABI 之后。 |
| L6 跨语言验证与打包 | 让共享向量、各语言测试/构建路径、覆盖率与制品边界保持可执行。 |

## L1 — 协议与身份

各运行时暴露七个固定 `Message` 字段：C++ 在 `cpp/include/epoch/engine.h:9`，Java 在 `java/src/main/java/io/epoch/Engine.java:14`，.NET 在 `dotnet/Engine.cs:5`，Go 在 `go/epoch.go:10`，Node 在 `node/src/index.ts:4`，Python 在 `python/epoch/engine.py:11`。`ActorIdParts` 与 codec 扩展点分别由 `cpp/include/epoch/actor_id.h:7`、`cpp/include/epoch/actor_id.h:15`，`java/src/main/java/io/epoch/ActorId.java:7`、`java/src/main/java/io/epoch/ActorId.java:10`，`dotnet/ActorId.cs:6`、`dotnet/ActorId.cs:19`，`go/actor_id.go:5`、`go/actor_id.go:14`，`node/src/actor-id.ts:1`、`node/src/actor-id.ts:9`，以及 `python/epoch/actor_id.py:11`、`python/epoch/actor_id.py:20` 表示。所有默认 codec 都执行相同的 10/12/6/10/26 位拆分，例如 Java 在 `java/src/main/java/io/epoch/ActorId.java:29`，Node 在 `node/src/actor-id.ts:15`。

仓库没有独立的机器可读 schema 文件。跨语言契约由 `docs/guide/protocol.md:10` 的文字规则加上各语言具体定义和测试组成。Node 有意让 `Message` 使用 `number`（`node/src/index.ts:4`），而 ActorId codec 使用 `BigInt`（`node/src/actor-id.ts:11`）；因此精度边界是 L1 API 约束，`docs/guide/protocol.md:25` 已记录该现状。

## L2 — 确定性 Epoch 处理

可移植操作是批次函数，不是常驻 Actor 运行时。它按 `(epoch, channelId, qos 降序, sourceId, sourceSeq)` 排序并把 payload 折叠进累积状态：Java 在 `java/src/main/java/io/epoch/Engine.java:56`，.NET 在 `dotnet/Engine.cs:58`，Go 在 `go/epoch.go:44`，Node 在 `node/src/index.ts:40`，Python 在 `python/epoch/engine.py:37`。C++ 在 `cpp/include/epoch/engine.h:25` 暴露同一契约，并在 `cpp/src/engine.cpp:14` 实现排序。

实现只拥有函数局部状态——排序后的输入、当前 Epoch、累积状态和结果列表；Java 见 `java/src/main/java/io/epoch/Engine.java:64`，Go 见 `go/epoch.go:50`，Node 见 `node/src/index.ts:51`，Python 见 `python/epoch/engine.py:42`。这里没有 Redis、MySQL 或文件持久化状态。每个 Epoch 边界都会用 FNV-1a 64 计算 `state:<state>`；Java 调用点在 `java/src/main/java/io/epoch/Engine.java:75`，.NET 在 `dotnet/Engine.cs:87`，Node 在 `node/src/index.ts:63`，Python 在 `python/epoch/engine.py:54`。行为契约位于 `docs/guide/behavior.md:15` 与 `docs/guide/behavior.md:27`。

## L3 — 传输边界与本地队列

传输契约有意保持三个操作：C++ 为 `Transport`（`cpp/include/epoch/transport.h:11`），Java 为 `io.epoch.Transport`（`java/src/main/java/io/epoch/Transport.java:5`），.NET 为 `ITransport`（`dotnet/Transport.cs:3`），Go 为 `Transport`（`go/transport.go:4`），Python 为 `Transport`（`python/epoch/transport.py:9`），Node 通过鸭子类型由 `InMemoryTransport`/`AeronTransport` 承载（`node/src/index.ts:115`、`node/src/index.ts:135`）。

`InMemoryTransport` 不是 Epoch 队列；它是 FIFO，不排序，也不按 Epoch 合并。各实现只在进程内存中保存消息：C++ deque 在 `cpp/include/epoch/transport.h:43`，Java deque 在 `java/src/main/java/io/epoch/InMemoryTransport.java:9`，.NET queue 在 `dotnet/Transport.cs:12`，Go slice 在 `go/transport.go:11`，Node array 在 `node/src/index.ts:116`，Python deque 在 `python/epoch/transport.py:22`。`close()` 只清空队列，不会阻止后续 send/poll；见 `java/src/main/java/io/epoch/InMemoryTransport.java:26` 和 `cpp/include/epoch/transport.h:37`。

## L4 — Aeron 投递适配器

C++ 是直接使用 Aeron C 客户端的适配器：`AeronConfig` 和 stats 声明在 `cpp/include/epoch/aeron_transport.h:12`、`cpp/include/epoch/aeron_transport.h:20`；context/client/publication/subscription 状态在 `cpp/include/epoch/aeron_transport.h:49`；offer 重试在 `cpp/src/aeron_transport.cpp:223`；fragment 消费在 `cpp/src/aeron_transport.cpp:284`；关闭时按 publication、subscription、client、context 顺序释放（`cpp/src/aeron_transport.cpp:316`）。

Java 有意独立实现，并且可以拥有嵌入式 Media Driver：配置默认值在 `java/src/main/java/io/epoch/AeronTransport.java:18`，driver/client/publication/subscription 构建在 `java/src/main/java/io/epoch/AeronTransport.java:68`，poll 接收行为在 `java/src/main/java/io/epoch/AeronTransport.java:188`。其余托管运行时调用原生网关，而不是复制 Aeron 生命周期：Go 在 `go/aeron_transport.go:144` 打开 C 句柄；Node 在 `node/src/aeron-native.ts:27` 加载 Koffi，并在 `node/src/index.ts:140` 打开句柄；.NET 在 `dotnet/AeronTransport.cs:240` 声明 P/Invoke；Python 在 `python/epoch/aeron_transport.py:127` 加载 ctypes，并在 `python/epoch/aeron_transport.py:195` 打开句柄。

所有适配器在跨越边界前都会把可移植消息映射为 Aeron Frame v1。native、C++、Java、Go、Node、Python 的编解码锚点分别是 `native/src/epoch_aeron.c:333`、`cpp/src/aeron_transport.cpp:127`、`java/src/main/java/io/epoch/AeronMessageCodec.java:22`、`go/aeron_transport.go:113`、`node/src/index.ts:84`、`python/epoch/aeron_transport.py:41`。线格式记录在 `docs/guide/transport-aeron.md:39`。

## L5 — 原生 Aeron 网关

`native/src/epoch_aeron.c` 拥有一个不透明 `epoch_aeron_transport` 对象。其生命周期状态包括 config、stats、closed 标记、Aeron context/client、publication、subscription 和接收缓冲区——声明在 `native/src/epoch_aeron.c:10`。`epoch_aeron_open` 通过初始化、启动、异步添加 publication/subscription 以及完成轮询来构建该状态（`native/src/epoch_aeron.c:130`）；失败路径通过 `epoch_aeron_close` 回收（`native/src/epoch_aeron.c:152`）。它不拥有嵌入式 Media Driver，也没有持久化存储；Aeron 配置目录仍在该对象之外。

ABI schema 正是 `native/include/epoch_aeron.h:14` 的 `epoch_aeron_config_t`、`native/include/epoch_aeron.h:24` 的 `epoch_aeron_stats_t`，以及 `native/include/epoch_aeron.h:37` 的五个函数。该头文件是 Go、Node、.NET、Python 共用的唯一协议边界；帧长也通过 `native/include/epoch_aeron.h:10` 的 `EPOCH_AERON_FRAME_LENGTH` 暴露。

## L6 — 跨语言验证与打包

规范向量是 `test-vectors/epoch_vector_v1.txt:1`；三类记录及其含义定义在 `docs/guide/protocol.md:42`。C++ 在 `cpp/CMakeLists.txt:29` 构建库和三个测试可执行文件；Java 的可执行向量 harness 在 `java/scripts/run-tests.sh:11` 编译 main 与 test 源码；Go 与 Python 复用共享文件，锚点在 `go/epoch_test.go:207` 和 `python/tests/test_version.py:42`；Node 通过 `node/package.json:10` 覆盖 version、codec、engine、内存传输和 native-frame 行为。

配置和生命周期输入按制品拆分。native 在 `native/CMakeLists.txt:4` 构建 Aeron 子模块，并在 `native/CMakeLists.txt:39` 构建共享库；C++ 在 `cpp/CMakeLists.txt:19` 静态链接同一子模块；Java 在 `java/scripts/fetch-aeron.sh:7` 下载固定版本 client/driver/Agrona jar 到缓存；Go 通过 `.github/workflows/build-go.yml:29` 的 `LD_LIBRARY_PATH` 依赖 native 构建产物；Node 在 `node/src/aeron-native.ts:49` 加载 Koffi，并在 `node/src/aeron-native.ts:152` 解析 `native/build`；Python 在 `python/epoch/aeron_transport.py:230` 优先使用 `EPOCH_AERON_LIBRARY`，再找仓库路径和系统库；.NET 在 `dotnet/AeronTransport.cs:242` 依赖平台加载器。CI 从 `.github/workflows/build-cpp.yml:1` 到 `.github/workflows/build-python.yml:1` 执行各语言工作流；`nightly.yml` 则从 `native` 到 Python 把已测试制品打包（`.github/workflows/nightly.yml:18`）。

## 拆分理由

1. **可移植语义与内存布局分离。** 同一排序和折叠算法独立存在于 Java（`java/src/main/java/io/epoch/Engine.java:56`）、.NET（`dotnet/Engine.cs:58`）、Go（`go/epoch.go:44`）、Node（`node/src/index.ts:40`）和 Python（`python/epoch/engine.py:37`）；共享向量可以在不强迫统一运行时 ABI 的情况下发现分叉。
2. **身份解析独立，因为它可替换且对精度敏感。** 各语言定义 codec 接口和默认实现：C++ 在 `cpp/include/epoch/actor_id.h:15`，Java 在 `java/src/main/java/io/epoch/ActorId.java:10`，.NET 在 `dotnet/ActorId.cs:19`，Go 在 `go/actor_id.go:14`，Node 在 `node/src/actor-id.ts:9`，Python 在 `python/epoch/actor_id.py:20`；Node 还把 `BigInt` codec 与 `number` message API 隔离（`node/src/actor-id.ts:11`、`node/src/index.ts:4`）。
3. **传输是显式接缝，确定性处理不依赖媒体状态。** C++ 的 `Transport` 头文件只引入 `epoch/engine.h`（`cpp/include/epoch/transport.h:3`）；Python 的 `Transport` 只引入 `Message`（`python/epoch/transport.py:6`）；Aeron 适配器实现同样三个方法，但不向上暴露 Publication/Subscription 类型。
4. **本地队列和网络适配器的所有权与关闭语义不同。** 内存存储只是进程内 FIFO（`java/src/main/java/io/epoch/InMemoryTransport.java:9`）；Aeron 要关闭 client 拥有的 publication、subscription、client 和 driver 资源（`java/src/main/java/io/epoch/AeronTransport.java:50`）。一个通用假抽象会掩盖资源和失败行为。
5. **Aeron 适配器在所有权边界处有意分叉。** C++ 直接拥有原生指针（`cpp/include/epoch/aeron_transport.h:49`），Java 拥有嵌入式 driver（`java/src/main/java/io/epoch/AeronTransport.java:68`），Go/Node/.NET/Python 把不透明状态委托给 `epoch_aeron`（`native/include/epoch_aeron.h:12`）。这样避免把 C client 生命周期复制进另外四个托管运行时。
6. **C ABI 只保留五个操作和两个结构。** `native/include/epoch_aeron.h:37` 是完整可调用面；Node 恰好声明这些绑定（`node/src/aeron-native.ts:49`），Python 在 `python/epoch/aeron_transport.py:127`，Go 在 `go/aeron_transport.go:65`，.NET 在 `dotnet/AeronTransport.cs:240`。
7. **线编解码耦合投递，而不是核心处理。** `processMessages` 只消费已解码的可移植消息，没有 frame 引用（`cpp/include/epoch/engine.h:25`、`python/epoch/engine.py:37`）；每个 Aeron 路径在适配器层以及 `native/src/epoch_aeron.c:333` 拥有 encode/decode。
8. **验证是一等层，因为行为等价是产品契约。** 同一个规范向量被 Java（`java/src/test/java/io/epoch/VectorTestMain.java:172`）、Go（`go/epoch_test.go:207`）、Python（`python/tests/test_version.py:42`）和 C++（`cpp/CMakeLists.txt:42`）独立解析；这能发现源码阅读无法保证的排序、哈希和整数差异。
9. **打包生命周期保持在运行时对象之外。** native 制品只复制构建库和 `epoch_aeron.h`（`.github/workflows/nightly.yml:44`）；Java 固定 jar 版本并生成 classpath（`java/scripts/fetch-aeron.sh:7`）；Python（`python/epoch/aeron_transport.py:230`）和 Node（`node/src/aeron-native.ts:152`）都把库发现与 `AeronTransport` 状态分开。
10. **测试钩子限定在适配器，不提升为公共传输策略。** C++ 把 `AeronHooks` 放在 `detail`（`cpp/include/epoch/aeron_transport.h:58`）；Go 替换 C 调用闭包（`go/aeron_transport.go:65`）；Node 和 Python 注入假 native 绑定（`node/src/index.ts:140`、`python/epoch/aeron_transport.py:195`）。失败注入始终靠近拥有外部依赖的一方。

## 文档落位

协议文字说明继续放在现有中文文档体系：`docs/guide/protocol.md` 负责字段、ActorId 和 frame 引用，`docs/guide/behavior.md` 负责排序和哈希规则，`docs/guide/transport-aeron.md` 负责 Aeron 生命周期和配置。站点由 `docs/.vuepress/config.js:1` 配置；除非后续增加导航项，本页不会自动出现在站点导航，本次也不修改站点配置。运行时代码中没有需要本地化的用户可见文案。
