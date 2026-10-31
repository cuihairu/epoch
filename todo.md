# Epoch TODO

- [x] README 表格数据行被并成单行，拆回每行一条（2026-10-27）
      README.md:25 的 2326 字符合并行按表格语义拆回 6 条数据行，列内容原样保留
      （6 行各 5 个 `|` / 4 列：C++17、Java 17、.NET 8.0、Go 1.24、Node.js 22、Python 3.12），
      并补回表格与 `## Nightly 构建` 标题之间的空行。
      该合并行是**本地未提交的脏文件**：拆分结果与 `main`（0012c75）中的 README.md 逐字节一致，
      因此 README.md 本身无提交差异；徽章行与版本号未动。
      验收：`awk 'length($0)>500 && /\|/' README.md` 输出为空（全仓库 .md 最长行 400 字符）。
      README.zh.md 在本仓库不存在（仅有 README.md / docs/README.md / docs/guide/README.md），
      其表格无此问题。

## 审核发现
- [x] 审核发现：SVG 箭头 marker 使用 `fill="context-stroke"`，ImageMagick 及部分 SVG 渲染器/旧浏览器不支持，可能导致箭头不可见（docs/public/images/architecture-en.svg:4）（commit: a50b85b）
- [x] 审核发现：`.vuepress/public/images/architecture-en.svg` 与 `docs/public/images/architecture-zh.svg` 在 diff 中无任何引用，属冗余副本，存在维护漂移风险（docs/.vuepress/public/images/architecture-en.svg:1）（commit: a268136）
- [x] 审核发现：SVG 中通道类型写作 "Latest"，与设计文档 `LatestValue` 命名不一致（docs/public/images/architecture-en.svg:66）（commit: b39e34a）
- [x] 审核发现：本轮 diff 未见问题（bf4f844..HEAD）。
      复核属实：范围含 4 个 docs 提交（a50b85b / a268136 / b39e34a / eea7ee3），
      均为文档修正——SVG 箭头改用显式颜色、删除经 `rg` 确认无引用的冗余副本、
      `LatestValue` 命名与 design 文档一致；两份 SVG 均通过 XML 解析，无功能/行为变更。
