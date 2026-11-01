# .NET 实现

- 版本：.NET 8.0
- 目录：`/dotnet`
- 测试：`dotnet test dotnet/Epoch.Tests/Epoch.Tests.csproj`

## Aeron
- 依赖：`native/build.sh` 构建 `epoch_aeron` 动态库
- 加载：经平台动态库搜索路径（Linux `LD_LIBRARY_PATH`、macOS `DYLD_LIBRARY_PATH`、Windows `PATH`）定位 `epoch_aeron`
- 绑定：P/Invoke 调用 `epoch_aeron`
