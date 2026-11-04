# 协议与传输

本节定义跨语言一致的协议结构与传输边界。

## Transport 接口（逻辑）
- 发送：`send(message)`
- 接收：`poll(max) -> [message]`
- 关闭：`close()`

## 消息最小字段
- `epoch`：逻辑时间片
- `channelId`：通道标识
- `sourceId`：来源标识（Actor/节点）
- `sourceSeq`：来源内单调递增序号
- `schemaId`：消息结构版本
- `qos`：优先级（0-255，越大越优先）
- `payload`：业务数据

## ActorId 约定（默认规则）
- `sourceId` 默认等于 `actorId`（设计约定；当前实现未强制校验，engine 仅透传字段）
- `actorId` 为 64-bit 数值，可按规则解析
- 默认布局（高位到低位）：
  - `region(10) | server(12) | processType(6) | processIndex(10) | actorIndex(26)`
- 解析规则可替换：实现 Codec 接口（encode/decode/name）即可切换（C++/Go/Python/Node.js 名为 `ActorIdCodec`，Java 为 `ActorId.Codec`，.NET 为 `IActorIdCodec`）
- Node.js 需要用 `BigInt` 保存完整 64-bit（现状：`Message` 字段仍为 `number`，超出安全整数范围会先丢精度，`node/src/index.ts:88-112`；bigint 迁移为破坏性变更，待设计拍板）

## 传输实现原则
- Java：独立实现 Transport（官方 Aeron 客户端）
- C++：基于 Aeron C 客户端（`third_party/aeron` submodule）
- C#/Go/Node/Python：经 `native/` 的 `epoch_aeron` 绑定（Aeron C）接入
- 必须保证可重放与一致性验证

## Aeron 集成
- 设计说明见：`docs/guide/transport-aeron.md`

## QoS 分级建议
- `240-255`: 系统/运维命令（如诊断、状态查询）
- `128-239`: 关键控制类消息
- `1-127`: 普通业务消息
- `0`: 默认

## 测试向量
- 必须提供“输入明细 + 期望 stateHash”的测试向量
- 所有语言实现都需复用该向量
- 基准文件：`test-vectors/epoch_vector_v1.txt`
  - `M,epoch,channelId,sourceId,sourceSeq,schemaId,payload`（qos 缺省为 0）
  - `M,epoch,channelId,sourceId,sourceSeq,schemaId,qos,payload`
  - `E,epoch,state,hash`
