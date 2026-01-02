# Node.js 实现

- 版本：Node.js 22
- 目录：`/node`
- 测试：`pnpm test`

## Aeron
- 依赖：`native/build.sh` 构建 `epoch_aeron` 动态库
- 环境变量：`EPOCH_AERON_LIBRARY` 指定动态库路径
- 绑定：koffi 调用 `epoch_aeron`
