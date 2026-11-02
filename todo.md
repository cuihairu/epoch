# Epoch TODO

## 文档一致性审计（2026-11-02）
- [x] 文档与源码一致性审计三桶六步执行，12 项差异全部修复（commit 1948936，docs build 16 页绿）
- [x] README 徽章/链接逐条可达验证，26 条均 200，徽章单行居中，logo 资产存在（commit 1948936）
- [x] 删除无引用资产 architecture-en.svg（commit 1948936）

## 代码深审（2026-11-01）
- [x] 安全静态扫描全仓 6 类，全部无发现（commit e9d087c）
- [x] 隐藏 bug 猎捕，发现 2 项 P2（commit e9d087c）
- [x] 质量门快照，C++/Go/Python/Node 全绿，.NET/Java 受环境限制（commit e9d087c）
- [x] 报告落 docs/review/2026-11-01-code-review.md（commit e9d087c）
- [x] P0/P1 当场修复，无适用项；P2/P3 记 todo（commit e9d087c）

## 代码深审（2026-11-02）
- [x] 安全静态扫描全仓 6 类，全部无发现
- [x] 隐藏 bug 猎捕，发现 3 项 P2（F1/F2 为上轮遗留、F3 为本轮新发现）
- [x] 质量门快照，C++/Go/Python/Node/Java 全绿，.NET 测试因环境限制未执行
- [x] 报告落 docs/review/2026-11-02-code-review-opencode.md
- [x] P0/P1 当场修复，无适用项；P2/P3 记 todo

## 待办（P2，记 todo）
- [x] 修复 C++ AeronTransport 构造器异常路径资源泄漏（context_/client_ 提前赋值使 catch 路径正确释放；新增 init/start 失败清理测试；ctest 3/3 绿）
- [ ] Node.js Message 64-bit 字段精度丢失，改 bigint 需设计拍板（node/src/index.ts:88-112）【BLOCKED：改 bigint 为破坏性 API 变更，需设计拍板】
- [x] Java AeronTransport 补充关闭状态守卫（volatile closed：send 后抛异常、poll 后空、close 幂等；run-tests.sh 绿）

## 审核发现
- [x] 审核发现：审计表 C1 称 architecture-en.svg「无任何页面引用」不准确——基线 README.md:5 实际引用了该资产，且其唯一引用与本删除在提交 1948936 同批移除，删除的是仍被引用的资产（已改写 docs/审计-文档一致性.md C1 与 logo 取证说明；pnpm run docs:build 绿）
- [x] 审核发现：protocol.md 列各语言 Codec 命名时遗漏 Node.js——node/src/actor-id.ts:9 同样定义 `ActorIdCodec` 类型（已补充 docs/guide/protocol.md Node.js 命名；cd node && pnpm test 绿）
- [x] 审核发现：C1 重新分类（码超前→不符）未同步更新段一三桶计数表——仍记「码超前=1 / 不符=1」，实际审计表为码超前=0、不符=2（已改为码超前=0、不符=2，docs/审计-文档一致性.md:13-14；pnpm run docs:build 绿）
- [x] 审核发现：段三总结仍称「1 项删除无引用资产」，与修正后 C1（资产被 README.md:5 引用、连同引用同批移除）口径矛盾（commit 75911f0 docs/审计-文档一致性.md:13-14；本项已改为「删除冗余资产并同步移除其唯一引用」docs/审计-文档一致性.md:25；pnpm run docs:build 绿）
- [x] 审核发现：close() 先置 closed=true 再 adapter.close()，若 adapter.close() 中途抛异常则资源仅部分释放、后续 close() 因 closed 已真而提前返回，泄漏无法重试（已改为 adapter.close() 成功后再置 closed=true，java/src/main/java/io/epoch/AeronTransport.java:165-172；新增首次 close 失败可重试用例 java/src/test/java/io/epoch/VectorTestMain.java:112-135；java run-tests.sh 绿）
- [x] 审核发现：C++ 构造器修复把 context_ 赋值提前至 context_set_dir 之前（正是为让 set_dir 失败时 close() 释放 context），但新增测试只覆盖 context_init/init/start 失败，未覆盖 context_set_dir 失败清理路径——stub_context_set_dir 恒返 0、无任何用例注入其失败（已让 stub_context_set_dir 支持注入失败，新增 test_aeron_context_set_dir_failure_cleans_up 断言 context 释放，cpp/tests/aeron_transport_test.cpp:67-80、:404-437、:466-470；ctest 3/3 绿）
- [ ] 审核发现：Java 关闭守卫为 volatile 非锁守卫，send/poll 检查 closed 后与并发 close() 存在 TOCTOU——线程读 closed==false 后 close() 已完成，该线程仍在已关闭 adapter 上 offer/poll，可能丢消息或抛异常（java/src/main/java/io/epoch/AeronTransport.java:128-171）；属尽力防护而非完全互斥，如需严格串行需 synchronized 包住 close+操作
