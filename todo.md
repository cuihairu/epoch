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
- [ ] 修复 C++ AeronTransport 构造器异常路径资源泄漏（cpp/src/aeron_transport.cpp:165-176）
- [ ] Node.js Message 64-bit 字段精度丢失，改 bigint 需设计拍板（node/src/index.ts:88-112）
- [ ] Java AeronTransport 补充关闭状态守卫（java/src/main/java/io/epoch/AeronTransport.java:114-166）

## 审核发现
- [x] 审核发现：审计表 C1 称 architecture-en.svg「无任何页面引用」不准确——基线 README.md:5 实际引用了该资产，且其唯一引用与本删除在提交 1948936 同批移除，删除的是仍被引用的资产（已改写 docs/审计-文档一致性.md C1 与 logo 取证说明；pnpm run docs:build 绿）
- [x] 审核发现：protocol.md 列各语言 Codec 命名时遗漏 Node.js——node/src/actor-id.ts:9 同样定义 `ActorIdCodec` 类型（已补充 docs/guide/protocol.md Node.js 命名；cd node && pnpm test 绿）
