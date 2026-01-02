# Python 实现

- 版本：Python 3.12
- 目录：`/python`
- 测试：`python -m unittest discover -s tests`

## Aeron
- 依赖：`native/build.sh` 构建 `epoch_aeron` 动态库
- 环境变量：`EPOCH_AERON_LIBRARY` 指定动态库路径
- 绑定：ctypes 调用 `epoch_aeron`
