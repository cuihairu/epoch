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
- `sourceId` 默认等于 `actorId`
- `actorId` 为 64-bit 数值，可按规则解析
- 默认布局（高位到低位）：
  - `region(10) | server(12) | processType(6) | processIndex(10) | actorIndex(26)`
- 解析规则可替换：实现 `ActorIdCodec`（encode/decode/name）即可切换
- Node.js 需要用 `BigInt` 保存完整 64-bit

## 传输实现原则
- 各语言独立实现 Transport，不依赖 C/C++ 绑定
- 可复用 Aeron 或自研 UDP 传输
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
