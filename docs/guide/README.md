# 适用边界（摘要）

## 文档结构
- 概念与设计：`/guide/concepts.html`
- 行为与一致性：`/guide/behavior.html`
- 协议与传输：`/guide/protocol.html`
- 语言实现：`/guide/languages.html`

## 适合的场景
- 单分区/单写权威逻辑（默认 A 模式）
- 强实时对抗与帧同步类游戏
- 顺序状态机类金融系统（可选 B 模式）

## 不适合的场景
- 通用分布式 Actor / 微服务平台
- 跨分区全局强序与强事务工作流
- 强依赖外部非确定性与阻塞 IO
