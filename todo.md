# Epoch TODO

## 文档一致性审计（2026-11-02）
- [x] 文档与源码一致性审计三桶六步执行，12 项差异全部修复（commit 1948936，docs build 16 页绿）
- [x] README 徽章/链接逐条可达验证，26 条均 200，徽章单行居中，logo 资产存在（commit 1948936）
- [x] 删除无引用资产 architecture-en.svg（commit 1948936）

## 代码深审（2026-11-02）
- [ ] 安全静态扫描全仓：硬编码密钥/shell 注入/SQL 拼接/eval/反序列化/路径穿越
- [ ] 隐藏 bug 猎捕：错误处理缺失/竞态/越界/资源泄漏/off-by-one/并发
- [ ] 质量门：lint/类型检查/测试基线对比
- [ ] 报告落 docs/review/2026-11-01-code-review.md
- [ ] P0/P1 当场修复（一问题一 commit，测试绿），P2/P3 记 todo
