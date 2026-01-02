# C++ 实现

- 版本：C++17
- 目录：`/cpp`
- 构建：`cmake -S cpp -B cppbuild && cmake --build cppbuild`
- 测试：`ctest --test-dir cppbuild --output-on-failure`

## Aeron
- 依赖：`third_party/aeron` submodule（Aeron C）
- 运行：需启动外置 Media Driver，`AeronTransport` 使用 `channel/stream_id/aeron_directory`
