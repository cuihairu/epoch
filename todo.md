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
