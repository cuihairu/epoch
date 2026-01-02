# Aeron 集成设计

本节描述 Aeron 传输层的集成方式与边界，Java/C++ 端已提供最小可用实现，C#/Go/Node/Python 通过 `epoch_aeron` 原生库绑定接入。

## 集成边界
- Runtime 只依赖 `Transport` 接口与协议语义
- Aeron 负责传输与重放基础能力（Publication / Subscription / Archive）
- QoS 在 Actor 层排序处理，传输层仅透传字段

## 进程内组件
- **Media Driver**：负责共享内存与网络 IO
- **Client**：Publisher / Subscriber
- **Archive（可选）**：回放与审计

## 启动模式
1. 外置 Media Driver（推荐生产）
2. Embedded Media Driver（开发/单机）

当前实现默认使用外置 Media Driver，Java 支持 embedded 模式（仅开发/单机）。C++ 端目前仅支持外置。

## 配置建议
- `channel`: Aeron channel（ipc/udp）
- `streamId`: stream 标识
- `endpoint`: `host:port`
- `termBufferLength`: 日志缓冲大小
- `lingerNs`: 发送尾部优化
- `mtuLength`: MTU
- `archiveEnabled`: 是否启用回放
- `aeronDirectory`: driver 目录
- `fragmentLimit`: 单次 poll 最大片段数（Java 默认 64）
- `offerMaxAttempts`: offer 重试上限（Java 默认 10）
- `idleStrategy`: 空转策略（Java 默认 BusySpin）
- `embeddedDriver`: 是否启用 embedded driver（Java）
- `dirDeleteOnStart/Shutdown`: embedded 模式是否清理目录（Java）

## 消息帧格式（v1）
固定长度 56 bytes，偏移如下：
- `0`: version (u8)
- `1`: qos (u8)
- `2-7`: reserved
- `8`: epoch (i64)
- `16`: channelId (i64)
- `24`: sourceId (i64)
- `32`: sourceSeq (i64)
- `40`: schemaId (i64)
- `48`: payload (i64)

## Debug / 观测出口
建议透出以下信息（与 Aeron counters 对齐）：
- Publication 位置、Backpressure 计数
- Subscription 进度、丢包计数
- Archive 录制/回放进度
- Error Log / Exception 事件

Java 端提供 `AeronTransport.stats()` 用于读取发送/接收/失败统计，C++ 端提供 `stats()`。

## 多语言实现策略
- Java：直接使用官方 Aeron 客户端
- C++：基于 Aeron C 客户端（git submodule）
- C#/Go/Node/Python：基于 `native/` 的 `epoch_aeron` 绑定（Aeron C）

## 原生库构建
```
./native/build.sh
```
默认输出在 `native/build`，可通过环境变量 `EPOCH_AERON_LIBRARY` 指定动态库路径。

## 交互流程（简版）
```
Client -> Registry: 查询 actorId -> endpoint
Client -> Aeron: Publication(channel, streamId)
Server -> Aeron: Subscription(channel, streamId)
```

> 当前提供最小可用实现，后续补充生产化配置与部署脚手架。 
