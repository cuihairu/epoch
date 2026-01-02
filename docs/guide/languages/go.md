# Go 实现

- 版本：Go 1.24
- 目录：`/go`
- 测试：`GOCACHE=.gocache go test ./...`

## Aeron
- 依赖：`native/build.sh` 构建 `epoch_aeron` 动态库
- 环境变量：`LD_LIBRARY_PATH`/`DYLD_LIBRARY_PATH` 指向 `native/build`
- 绑定：cgo 调用 `epoch_aeron`
- 构建：需启用 `CGO_ENABLED=1`
