# .NET 实现

- 版本：.NET 8.0
- 目录：`/dotnet`
- 测试：`dotnet test dotnet/Epoch.Tests/Epoch.Tests.csproj`

## Aeron
- 依赖：`native/build.sh` 构建 `epoch_aeron` 动态库
- 环境变量：`EPOCH_AERON_LIBRARY` 指定动态库路径
- 绑定：P/Invoke 调用 `epoch_aeron`
