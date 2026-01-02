# Aeron 集成设计（规划）

本节描述 Aeron 传输层的集成方式与边界，Java 端已提供最小可用实现，其余语言保持骨架。

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

当前实现默认使用外置 Media Driver，Java 支持 embedded 模式（仅开发/单机）。

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

Java 端提供 `AeronTransport.stats()` 用于读取发送/接收/失败统计。

## 多语言实现策略
- Java：直接使用官方 Aeron 客户端
- C++/C#/Go/Node/Python：
  - 方案 A：基于 Aeron C 库绑定（推荐短期）
  - 方案 B：原生实现 UDP 传输并遵循协议（长期）

## 交互流程（简版）
```
Client -> Registry: 查询 actorId -> endpoint
Client -> Aeron: Publication(channel, streamId)
Server -> Aeron: Subscription(channel, streamId)
```

> 当 Aeron 集成落地后，此文档将升级为可运行配置与脚手架指引。 
