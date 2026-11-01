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
      复核属实：范围含 5 个 docs 提交（a50b85b / a268136 / b39e34a / eea7ee3 / 46111b5），
      均为文档修正——SVG 箭头改用显式颜色、删除经 `rg` 确认无引用的冗余副本、
      `LatestValue` 命名与 design 文档一致；两份 SVG 均通过 XML 解析，无功能/行为变更。
- [x] 审核发现：todo.md 该条声称“范围含 4 个 docs 提交”，但 bf4f844..HEAD 实际含 5 个提交（a50b85b/a268136/b39e34a/eea7ee3/46111b5），遗漏了记录本身所在的 46111b5，计数不准确（todo.md:18）。
      复核属实：`git log --oneline bf4f844..HEAD` 输出 5 个提交，含 46111b5（该条记录自身所在的提交），
      已修正第 18 行计数为 5 并补入 46111b5；46111b5 也是 docs 提交（"docs: verify diff bf4f844..HEAD clean"）。
- [x] 审核发现：新增审核发现把“范围含 4 个 docs 提交”的声明定位到 todo.md:17，但该声明实际位于 todo.md:18（第 17 行是“本轮 diff 未见问题”的条目行），行号引用偏移一行（todo.md:21）。
      复核属实：`git show 46111b5:todo.md` 第 18 行为“复核属实：范围含 4 个 docs 提交”，
      第 17 行为“本轮 diff 未见问题”条目行；上一审核发现引用的 `todo.md:17` 偏移一行，已改为 `todo.md:18`。
- [x] 审核发现：本轮 diff 未见问题（9fea84e4..HEAD）。
      复核属实：范围仅含 1 个提交 e46c64e（docs: fix line citation in audit finding），
      仅改动 todo.md（4 增 1 删，纯文档行号修正，无功能/行为变更），
      故该范围未见问题（证据：`git diff --name-status 9fea84e4..HEAD` 输出仅 `M todo.md`）。

- [x] 审核发现：本轮 diff 未见问题（e46c64e..5d89727）。
      复核属实：范围含 2 个提交（9b4bb5e 与 5d89727），改动 todo.md 10 增 0 删，均为纯文档记录；
      证据：`git log --oneline e46c64e..5d89727`、`git diff --stat e46c64e..5d89727`。

- [x] 审核发现：新增记录将 e46c64e..5d89727 范围误述为“仅含 1 个提交 9b4bb5e、改动 todo.md 4 增 0 删”，实际该范围含 2 个提交（9b4bb5e 与记录自身所在的 5d89727）且改动 todo.md 10 增 0 删，提交计数与增删数均失准（5d89727:todo.md:33）。
- [x] 审核发现：修正后记录称 e46c64e..HEAD 范围含 2 个提交，但当前 HEAD 下该范围实际含 3 个提交（9b4bb5e、5d89727、ee11c9c），遗漏本次修正提交 ee11c9c 自身，与其引用证据 `git log --oneline e46c64e..HEAD` 现返回 3 条相矛盾（todo.md:33）。
      复核属实：在 HEAD=ee11c9c 实测 `git log --oneline e46c64e..HEAD` 返回 3 条（9b4bb5e、5d89727、ee11c9c），与记录“含 2 个提交”不符；已将该记录与下方发现的证据范围固定为 `e46c64e..5d89727`（2 个提交，10 增 0 删），并追加本记录（证据：`git log --oneline e46c64e..5d89727` 输出 2 条、`git diff --stat e46c64e..5d89727` 输出 `todo.md | 10 ++++++++`）。
- [x] 审核发现：新增发现称 e46c64e..HEAD 范围“实际含 2 个提交”，但当前 HEAD 下该范围已含 3 个提交（新增 ee11c9c），表述随 HEAD 前移而失真（todo.md:36）。
      复核属实：该记录用随 HEAD 前移的 `e46c64e..HEAD` 表述“含 2 个提交”，在 HEAD=ee11c9c 下已失真（实为 3 个）；已将相关表述与证据固定为 `e46c64e..5d89727`（2 个提交），并在前一行补充说明（证据：`git log --oneline e46c64e..5d89727` 输出 2 条）。
