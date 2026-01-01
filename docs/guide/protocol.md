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
- `payload`：业务数据

## 传输实现原则
- 各语言独立实现 Transport，不依赖 C/C++ 绑定
- 可复用 Aeron 或自研 UDP 传输
- 必须保证可重放与一致性验证

## 测试向量
- 必须提供“输入明细 + 期望 stateHash”的测试向量
- 所有语言实现都需复用该向量
- 基准文件：`test-vectors/epoch_vector_v1.txt`
  - `M,epoch,channelId,sourceId,sourceSeq,schemaId,payload`
  - `E,epoch,state,hash`
