# Epoch

基于 Aeron 的高性能 Actor / Channel Runtime，面向游戏与金融场景，强调确定性、可复现、低延迟抖动和高可推理性。

## 设计哲学
- 时间为第一公民 (Epoch-driven)
- Actor 状态封闭、顺序执行
- Channel 提供确定性消息传递
- ECS（Entity-Component-System）用于 CPU 热路径优化

## 核心概念（简版）
- **Epoch**: 明确的逻辑时间区间，消息按 Epoch 分类与处理
- **Actor**: 固定线程、Epoch 内顺序执行，状态在边界可见
- **Channel**: 消息入队，不立即触发执行；结合 Epoch 保持确定性
- **ECS**: 数据布局优化 CPU cache，支撑批处理更新

## 适用边界（摘要）
- 适合：单分区/单写权威逻辑、帧同步/强实时对抗、顺序状态机类撮合与风控
- 不适合：通用分布式 Actor 平台、跨分区全局强序、强依赖外部非确定性

## 支持语言

| Language | Version | Build | Coverage |
|---|---|---|---|
| ![C++](https://img.shields.io/badge/C%2B%2B-00599C?logo=c%2B%2B&logoColor=white) | C++17 | [![build-cpp](https://github.com/cuihairu/epoch/actions/workflows/build-cpp.yml/badge.svg)](https://github.com/cuihairu/epoch/actions/workflows/build-cpp.yml) | [![codecov](https://codecov.io/gh/cuihairu/epoch/branch/main/graph/badge.svg?flag=cpp)](https://codecov.io/gh/cuihairu/epoch) |
| ![Java](https://img.shields.io/badge/Java-007396?logo=openjdk&logoColor=white) | Java 17 | [![build-java](https://github.com/cuihairu/epoch/actions/workflows/build-java.yml/badge.svg)](https://github.com/cuihairu/epoch/actions/workflows/build-java.yml) | [![codecov](https://codecov.io/gh/cuihairu/epoch/branch/main/graph/badge.svg?flag=java)](https://codecov.io/gh/cuihairu/epoch) |
| ![.NET](https://img.shields.io/badge/.NET-512BD4?logo=dotnet&logoColor=white) | .NET 8.0 | [![build-dotnet](https://github.com/cuihairu/epoch/actions/workflows/build-dotnet.yml/badge.svg)](https://github.com/cuihairu/epoch/actions/workflows/build-dotnet.yml) | [![codecov](https://codecov.io/gh/cuihairu/epoch/branch/main/graph/badge.svg?flag=dotnet)](https://codecov.io/gh/cuihairu/epoch) |
| ![Go](https://img.shields.io/badge/Go-00ADD8?logo=go&logoColor=white) | Go 1.24 | [![build-go](https://github.com/cuihairu/epoch/actions/workflows/build-go.yml/badge.svg)](https://github.com/cuihairu/epoch/actions/workflows/build-go.yml) | [![codecov](https://codecov.io/gh/cuihairu/epoch/branch/main/graph/badge.svg?flag=go)](https://codecov.io/gh/cuihairu/epoch) |
| ![Node.js](https://img.shields.io/badge/Node.js-339933?logo=node.js&logoColor=white) | Node.js 22 | [![build-node](https://github.com/cuihairu/epoch/actions/workflows/build-node.yml/badge.svg)](https://github.com/cuihairu/epoch/actions/workflows/build-node.yml) | [![codecov](https://codecov.io/gh/cuihairu/epoch/branch/main/graph/badge.svg?flag=node)](https://codecov.io/gh/cuihairu/epoch) |
| ![Python](https://img.shields.io/badge/Python-3776AB?logo=python&logoColor=white) | Python 3.12 | [![build-python](https://github.com/cuihairu/epoch/actions/workflows/build-python.yml/badge.svg)](https://github.com/cuihairu/epoch/actions/workflows/build-python.yml) | [![codecov](https://codecov.io/gh/cuihairu/epoch/branch/main/graph/badge.svg?flag=python)](https://codecov.io/gh/cuihairu/epoch) |

## 文档
- 概念与设计：`docs/guide/concepts.md`
- 行为与一致性：`docs/guide/behavior.md`
- 协议与传输：`docs/guide/protocol.md`
- 语言实现：`docs/guide/languages.md`
- 设计文档（完整）：`docs/guide/epoch-actor-design.md`
- 站点文档（VuePress v2）：`docs/`

## 本地预览
```bash
corepack enable
pnpm install
pnpm run docs:dev
```

## Aeron 原生库
```bash
./native/build.sh
```
非 Java 语言默认从 `native/build` 加载 `epoch_aeron`，或通过 `EPOCH_AERON_LIBRARY` 指定路径。
