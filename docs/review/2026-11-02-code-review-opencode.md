# 代码深审（2026-11-02）

> 依据用户任务书：①安全静态扫描全仓 ②隐藏 bug 猎捕 ③质量门 ⑤P0/P1 当场修、P2/P3 记 todo ⑥汇报。
> 方法：code-review 技能流程（每条发现带 file:line 证据，逐条 VERIFIED；不转述未经核验的发现）。
> 范围：全仓源码 `cpp/ go/ java/ dotnet/ node/src/ python/epoch/ native/`（排除 node_modules/dist/build 产物与 third_party 锁定依赖源码外围）。

## 结论

- **P0：0 项**
- **P1：0 项**
- **P2：3 项**（F1、F2 为 2026-11-01 轮次遗留未修；F3 为本轮（2026-11-02）新发现）
- **P3：0 项**
- **发现 3 / 已修 0 / 剩余 3**；安全静态扫描 6 类全部无发现。

## 一、安全静态扫描（6 类，全部无发现）

| 类别 | 结果 | 证据 |
|---|---|---|
| 硬编码密钥/凭据 | 无 | grep 密钥名模式（password/secret/api_key/token/private_key/access_key/credential/bearer/-----BEGIN…/AKIA…）全仓源码（排除 node_modules/dist/build/bin/obj/coverage 与 third_party 外围）0 命中 |
| Shell 注入 | 无 | 源码 grep `system/popen/exec/spawn/subprocess/child_process/Process.Start/os.system/shell.exec` 0 命中；仅 `cross-spawn@7.0.6` 出现在 `pnpm-lock.yaml`（依赖元数据，非执行代码） |
| SQL 拼接 | 无 | 全仓源码 0 命中（无 SQL/数据库代码路径） |
| eval / 动态执行 | 无 | grep `eval(/new Function/__import__/importlib/exec/execSync/vm.` 0 命中 |
| 反序列化 | 无 | grep `pickle/yaml.load/marshal/ObjectInputStream/readObject/unmarshal` 0 命中（仅 .NET `Marshal.StringToHGlobalAnsi` 为原生互操作，非数据反序列化） |
| 路径穿越 | 无 | 全仓源码用户可控路径拼接 0 命中；原生库加载统一走固定相对路径 + `EPOCH_AERON_LIBRARY` 环境变量 + 有限向上遍历（≤6 级），无 `../` 输入拼接 |

补充（非安全类别、非缺陷）：native C 层无 `strcpy/strcat/gets`，仅 `snprintf`（有界）与 `memcpy`（56B 帧长有界）；无格式化字符串注入（格式串均为常量）。

## 二、隐藏 bug 猎捕（3 项，均为 P2，按严重度排序）

### F1 · P2 —— C++ AeronTransport 构造器异常路径资源泄漏（2026-11-01 遗留，本轮复核确认仍存）

- **file:line**：`cpp/src/aeron_transport.cpp:164-176`（context/client 局部变量生命周期）
- **证据**：
  - `context` 在 `aeron_context_init`（:164）分配后，**`:171` 才执行 `context_ = context`**；若 `aeron_context_set_dir`（:168-169）抛异常 → catch（:219）→ `close()`（:314）检查 `context_ != nullptr`（:337）为假 → 已分配的 `context` 永不释放。
  - `client` 在 `aeron_init`（:174）分配后，**`:176` 才执行 `client_ = client`**；若 `aeron_start`（:175）抛异常 → catch（:219）→ `close()` 因 `client_ == nullptr`（:332）跳过 `aeron_close`（:334）→ 局部 `client` 泄漏。
- **触发**：Aeron 目录无效/驱动未启动时 `aeron_context_set_dir` / `aeron_start` 失败 → 构造器抛 `runtime_error`，且泄漏底层 context/client。
- **影响**：错误路径资源泄漏（内存/句柄）；正常路径（成功初始化）不受影响。
- **置信度**：VERIFIED（代码路径逐行核验；native C 层 `epoch_aeron.c:171/178/197` 为即时赋值，C++ wrapper 延迟赋值为引入点）。
- **状态**：未修（`todo.md`「待办」项仍在，P2 记 todo 策略）。

### F2 · P2 —— Node.js Message 64-bit 字段精度丢失（2026-11-01 遗留，本轮复核确认仍存）

- **file:line**：`node/src/index.ts:4-12`（Message 类型）、`:88-93`（encode）、`:105-112`（decode）
- **证据**：
  - `Message` 各字段类型为 JS `number`（`node/src/index.ts:4-12`）；
  - encode：`buffer.writeBigInt64LE(BigInt(message.epoch), 8)`（:88）——`message.epoch` 已是 `number`，> 2^53 时先丢精度再转 BigInt；
  - decode：`Number(buffer.readBigInt64LE(8))`（:105）——64-bit 值 > 2^53 时 `Number()` 静默舍入。
  - 触发实例：`encodeActorId({region:512,server:0,processType:0,processIndex:0,actorIndex:0})`（`node/src/actor-id.ts:35-54`）= `512n<<54n = 2^63n = 9223372036854775808n`；经 `Message.sourceId` number 字段 → `BigInt(...)`/decode 后均损坏。
- **影响**：region ≥ 512（10-bit region 合法上限）时 sourceId 静默损坏；`docs/guide/protocol.md` 已明确「Node.js 需要用 BigInt 保存完整 64-bit」。属 API 破坏性变更，需设计拍板后改（bigint 迁移）。
- **置信度**：VERIFIED（代码 + 具体数值触发复核）。
- **状态**：未修（`todo.md` 待办；P2 记 todo）。

### F3 · P2 —— Java AeronTransport 缺少关闭状态守卫（本轮新发现）

- **file:line**：`java/src/main/java/io/epoch/AeronTransport.java:114-166`
- **证据**：
  - 无 `closed` 成员字段（对比 C++（`cpp/src/aeron_transport.cpp:233` `closed_`）/ Go（`go/aeron_transport.go:41` `closed`）/ Node（`node/src/index.ts:138` `closed`）/ Python（`python/epoch/aeron_transport.py:199` `_closed`）/ .NET（`dotnet/AeronTransport.cs:31` `closed`）均有守卫；
  - `Send`（:126-142）无关闭检查；`Poll`（:145-159）无关闭检查；`close()`（:162-164）无幂等守卫，直接调 `adapter.close()`。
- **触发**：transport 关闭后再 `send`/`poll`，或重复 `close`。
- **影响**：关闭态未守卫；重复 close / 关闭后操作可能对已释放句柄执行操作或抛异常；跨语言生命周期语义不一致。
- **置信度**：VERIFIED（逐行核验）。
- **状态**：新发现，建议记 todo；严重度 P2（健壮性，非安全、非数据损坏）。

## 三、质量门（2026-11-02 基线）

| 语言 | lint/类型 | 构建 | 测试 | 基线 |
|---|---|---|---|---|
| C++ | `g++ -std=c++17 -Wall -Wextra -Werror`（项目代码 3 源文件）0 告警 | `cmake -S cpp -B cppbuild && cmake --build` 通过；`ctest` 3/3 通过 | `cpp/tests/*` 3 用例 | **全绿**（第三方 a 类 const 告警仅来自 third_party，非项目代码） |
| Go | `go vet ./...` 通过；`go build` 通过 | — | `go test ./...` 17/17 通过（需 `LD_LIBRARY_PATH=native/build`）；`go test -race ./...` 通过 | **全绿** |
| Python | `python -m compileall` 通过 | — | `python -m unittest discover` 10/10 通过 | **全绿** |
| Node | `npx tsc -p tsconfig.json` 通过 | — | `node --test` 11/11 通过 | **全绿** |
| .NET | `dotnet build` 0 警告 0 错误 | `Epoch -> bin/Debug/net8.0/Epoch.dll` 成功 | **未执行**——仅装 .NET 10.0.12 运行时，缺 net8.0 运行时（testhost 要求 8.0） | 构建绿；测试因环境限制未执行 |
| Java | `javac --release 17`（经 `scripts/run-tests.sh`，拉取 Aeron jar）通过；`VectorTestMain` 退出 0（全绿） | — | `java ... io.epoch.VectorTestMain` 通过 | **全绿** |

- 基线说明：本轮为首次全仓质量门快照（同轮内各语言均重跑）；.NET 测试未执行为环境限制（缺 net8.0 运行时），Java 测试已通过本地脚本完成。

## 四、修复执行（P0/P1 当场修）

- 本轮扫描未发现 **P0 / P1** 项（3 项发现均为 P2）。
- **P0/P1 当场修：无适用项，0 commit**（P2/F1、F2、F3 按既定策略记 todo，未改码）。

## 五、汇报（总结）

- **发现数**：3（P2：F1、F2、F3）
- **已修数**：0
- **剩余数**：3（F1、F2 待设计拍板后修；F3 建议转 todo 跟进）
- P0/P1 当场修：无适用项，0 commit。
- 安全扫描：6 类全部无发现。
- 质量门：C++/Go/Python/Node/Java 全绿；.NET 构建绿（测试因缺 net8.0 运行时未执行）。
- commit 清单：
  - `docs: add code review report` — 本报告落地 `docs/review/2026-11-02-code-review-opencode.md`
  - `docs: record code review findings in todo` — F3 新发现转 todo 登记
