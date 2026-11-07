# 术语表

本表定义同步架构页使用的源码级术语：[英文架构](../../en/guide/architecture.md)与[中文架构](architecture.md)。英文术语表为 [Glossary](../../en/guide/glossary.md)。

| 术语 | 英文 | 定义 | 证据 |
|---|---|---|---|
| Message | Message | 七字段可移植记录：epoch、channelId、sourceId、sourceSeq、schemaId、QoS、payload。 | `cpp/include/epoch/engine.h:9` |
| ActorIdParts | ActorIdParts | 解码后的身份字段：region、server、processType、processIndex、actorIndex。 | `cpp/include/epoch/actor_id.h:7` |
| ActorIdCodec | ActorIdCodec | 可替换的 encode/decode 策略，在 `ActorIdParts` 与 64-bit ID 间转换；Java 暴露为 `ActorId.Codec`，.NET 为 `IActorIdCodec`。 | `cpp/include/epoch/actor_id.h:15` |
| 默认 ActorId 布局 | Default ActorId layout | 默认高位到低位拆分：region 10 位、server 12 位、processType 6 位、processIndex 10 位、actorIndex 26 位。 | `java/src/main/java/io/epoch/ActorId.java:29` |
| Epoch 处理 | Epoch processing | 按确定性键排序批次，并把 payload 折叠进累积状态。 | `java/src/main/java/io/epoch/Engine.java:56` |
| Epoch 边界 | Epoch boundary | 处理下一个不同 Epoch 前，输出累积状态的时机。 | `java/src/main/java/io/epoch/Engine.java:74` |
| EpochResult | EpochResult | 一条输出记录，包含 Epoch 号、累积状态和十六进制状态哈希。 | `cpp/include/epoch/engine.h:19` |
| stateHash | stateHash | 对 `state:<state>` 计算 FNV-1a 64，并输出 16 位小写十六进制。 | `java/src/main/java/io/epoch/Engine.java:46` |
| Transport | Transport | send、poll、close 三个操作构成的边界，隔离投递与处理。 | `cpp/include/epoch/transport.h:11` |
| InMemoryTransport | InMemoryTransport | 进程内 FIFO 队列，用于直接消息交接；不按 Epoch 排序，也不持久化。 | `java/src/main/java/io/epoch/InMemoryTransport.java:8` |
| AeronTransport | AeronTransport | 由 Publication/Subscription 支撑的 `Transport` 实现；各语言显式拥有或委托原生资源。 | `java/src/main/java/io/epoch/AeronTransport.java:17` |
| AeronConfig | AeronConfig | 投递配置：channel、streamId、Aeron 目录、fragment limit 和 offer 重试上限；Java 增加嵌入式 driver 配置。 | `cpp/include/epoch/aeron_transport.h:12` |
| AeronStats | AeronStats | 发送、接收和分类 offer 失败的投递计数。 | `native/include/epoch_aeron.h:24` |
| Aeron Frame v1 | Aeron Frame v1 | 单条可移植消息的固定 56 字节小端表示。 | `docs/guide/transport-aeron.md:39` |
| 原生网关 | Native gateway | 不透明 C 对象与五函数 ABI，为 Go、Node、.NET、Python 集中管理 Aeron C client 生命周期。 | `native/include/epoch_aeron.h:37` |
| Media Driver | Media Driver | 用于共享内存和网络 IO 的 Aeron 组件；Java 可嵌入式启动，原生网关则外部委托。 | `java/src/main/java/io/epoch/AeronTransport.java:68` |
| 规范向量 | Canonical vector | 共享的 `test-vectors/epoch_vector_v1.txt` 输入与期望 Epoch 结果，用于比较各实现。 | `test-vectors/epoch_vector_v1.txt:1` |
| Vector harness | Vector harness | 各语言可执行程序，解析规范文件并断言等价状态演进。 | `java/src/test/java/io/epoch/VectorTestMain.java:172` |
| 覆盖率生命周期 | Coverage lifecycle | 各语言插桩和报告步骤，把行为证据关联到源码。 | `.github/workflows/build-go.yml:36` |
| 源码层 | Source layer | 依据所有权、调用方向、状态和生命周期归纳出的职责边界，不是目录名。 | `docs/en/guide/architecture.md:5` |
