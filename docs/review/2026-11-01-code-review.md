# 代码深审（2026-11-01）

> 依据用户任务书：①安全静态扫描全仓 ②隐藏 bug 猎捕 ③质量门 ④报告落此文件 ⑤P0/P1 当场修、P2/P3 记 todo ⑥汇报。
> 方法：code-review 技能流程（每条发现带 file:line 证据，逐条 VERIFIED；不转述未经核验的发现）。
> 范围：全仓源码 `cpp/ go/ java/ dotnet/ node/src/ python/epoch/ native/`（排除 node_modules/dist/build 产物）。

## 结论

- **P0：0 项**
- **P1：0 项**
- **P2：2 项**（均记 todo，未改码）
- **P3：0 项**
- **发现 2 / 已修 0 / 剩余 2**；安全静态扫描 6 类全部无发现。

## 一、安全静态扫描（6 类，全部无发现）

| 类别 | 结果 | 证据 |
|---|---|---|
| 硬编码密钥/凭据 | 无 | grep password/secret/api_key/token/private_key/BEGIN… 全仓源码为空 |
| Shell 注入 | 无 | 仅 `node/src/aeron-native.ts:28` 的 `require("koffi")` FFI（非 shell）；无 system/popen/subprocess/exec/Process.Start |
| SQL 拼接 | 无 | 无 SELECT/INSERT/UPDATE/sqlite/jdbc 字符串拼接 |
| eval / 动态执行 | 无 | 无 `eval(`/`new Function(`/`__import__`/`importlib` |
| 反序列化 | 无 | 无 pickle/yaml.load/marshal/ObjectInputStream/readObject |
| 路径穿越 | 无 | 唯一文件路径为硬编码 `native/build`（FFI 加载）与 `EPOCH_AERON_LIBRARY` 环境变量；无 `../` 用户输入拼接 |

补充：native C 层无 `strcpy/strcat/gets`，仅 `snprintf`（bounded, `epoch_aeron.c:32,46`）与 `memcpy`（bounded 56B, `epoch_aeron.c:300`）；无格式化字符串注入（格式串均为常量）。

## 二、隐藏 bug 猎捕（2 项，均为 P2）

### F1 · P2 —— C++ AeronTransport 构造器异常路径资源泄漏

- **file:line**：`cpp/src/aeron_transport.cpp:165-176`
- **证据**：
  - `context` 在 `aeron_context_init`（:165）分配，但 `context_ = context` 延迟到 :171；若 `aeron_context_set_dir`（:167-169）抛异常，catch（:219）→ `close()`（:314）检查 `context_ != nullptr`（:337）为假，已分配的 `context` 永不释放。
  - `client` 在 `aeron_init`（:174）分配，但 `client_ = client` 延迟到 :176；若 `aeron_start`（:175）抛异常，`client_` 仍为 nullptr，`client` 泄漏。
- **触发**：Aeron 目录无效/驱动未启动时 `aeron_context_set_dir` 或 `aeron_start` 失败 → 构造器抛 `runtime_error` 且泄漏底层资源。
- **影响**：错误路径资源泄漏；正常路径（成功初始化）不受影响。非安全、非崩溃。
- **置信度**：VERIFIED（代码路径逐行核验；native C 对照 `epoch_aeron.c:140-217` 即时存指针无此问题）
- **修法**：分配后立即存入成员指针（`context_ = context` 紧跟 :165；`client_ = client` 紧跟 :174），或在 catch 中按局部变量释放。

### F2 · P2 —— Node.js 消息 64-bit 字段精度丢失

- **file:line**：`node/src/index.ts:88-93`（encode）、`:105-112`（decode）
- **证据**：
  - `Message` 的 `epoch/channelId/sourceId/sourceSeq/schemaId/payload` 类型为 `number`（`node/src/index.ts:4-12`）；
  - encode：`buffer.writeBigInt64LE(BigInt(message.epoch), 8)`（:88）——`message.epoch` 已是 `number`，> 2^53 时先丢精度再转 BigInt；
  - decode：`Number(buffer.readBigInt64LE(8))`（:105）——64-bit 值 > 2^53 时 `Number()` 静默舍入；
  - 触发实例：`encodeActorId({region:512,server:0,processType:0,processIndex:0,actorIndex:0})`（`node/src/actor-id.ts:35-54`）= `512n<<54n = 0x8000000000000000n = 9223372036854775808n`；`Number(…n)` → `9223372036854776000`，再 `BigInt(…)` 与原值不等。
- **影响**：region ≥ 512（10-bit region 的合法值）时 sourceId 静默损坏 → 排序键 `(…, sourceId, …)` 错序 → 跨语言 stateHash 不一致、确定性被破坏。当前测试向量 sourceId 均 < 2^53（`test-vectors/epoch_vector_v1.txt`），未被现有测试覆盖。
- **置信度**：VERIFIED（代码 + 具体数值触发复核）
- **修法**：Node `Message` 的 64-bit 字段改 `bigint`（与 `protocol.md` 的「Node 需用 BigInt 保存 64-bit」一致，`node/src/actor-id.ts:11` 已用 bigint）；或 API 层强校验拒绝 > 2^53。属 API 破坏性变更，需设计拍板后改。

## 三、质量门（lint / 类型 / 测试基线）

| 语言 | lint/类型 | 测试 | 基线 |
|---|---|---|---|
| C++ | `g++ -Wall -Wextra -Werror` 项目代码 0 告警（6 条告警均来自 third_party/aeron）；cmake 构建通过 | `ctest` 3/3 通过 | `cpp/tests/*` 3 用例 |
| Go | `go vet ./...` 通过；`go build` 通过 | `go test ./...` 17/17 通过 | `go/*_test.go` 17 用例 |
| Python | `python -m compileall` 通过 | `unittest discover` 10/10 通过 | `python/tests/*` 10 用例 |
| Node | `tsc --noEmit` 通过 | `node --test` 11/11 通过（7 version + 4 aeron） | `node/test/*` 11 用例 |
| .NET | `dotnet build` 0 警告 0 错误 | **未执行**——仅装 .NET 10.0.12 运行时，缺 net8.0 运行时（testhost 要求 8.0） | `dotnet/Epoch.Tests` |
| Java | `javac --release 17` 因缺 Aeron 依赖（`io.aeron.*`）不可本地编译 | **未执行**——`mvn` 未安装，无 gradle wrapper | `java/src/test/.../VectorTestMain.java` |

- .NET/Java 测试未执行为环境限制（缺运行时/缺 Maven），非代码缺陷；构建侧 .NET 0 警告、C++/Go/Python/Node 全部绿。
- 基线对比：本轮为首次全仓质量门快照，无历史基线可比；各语言测试计数见上表。

## 四、观察项（非缺陷，记录备查）

- **native `epoch_aeron_poll` 帧数契约脆弱**：`frame_capacity` 语义为「帧数」（调用方分配 `capacity*56` 字节，`epoch_aeron.c:321-335`）。当前所有调用方（go/python/node/dotnet）均按帧数传参且正确分配（如 `go/aeron_transport.go:86`、`python/.../aeron_transport.py:119`），无越界；但契约未文档化，误传字节数会越界写。建议在头文件注释明确。
- **stats 计数非原子**：`sent_count/received_count` 等自增无锁（`epoch_aeron.c:246,349`）。按架构「Actor 状态封闭、单线程」（`docs/guide/behavior.md`）设计成立；若跨线程并发 send/poll 则竞态。属设计约定，非缺陷。
- **InMemoryTransport 非线程安全**：`go/transport.go:10-32`、`java/InMemoryTransport.java:8-29` 队列无锁；测试/单线程传输，按设计单线程使用。非缺陷。
- **`AERON_VERSION_*` CMake 宏未在代码中使用**：`cpp/CMakeLists.txt:29-33` 定义后无源码引用（grep 为空）。冗余配置，无害。

## 五、汇报

- 发现数：2（P2：F1、F2）｜已修数：0（无 P0/P1）｜剩余数：2（均记 todo）
- P0/P1 当场修：无适用项，0 commit。
- 安全扫描：6 类全部无发现。
- 质量门：C++/Go/Python/Node 全绿；.NET 构建绿（测试因缺 net8.0 运行时未执行）；Java 构建不可行（缺 Maven）。
- commit 清单：无代码修复提交（本报告仅新增文档，见 git log `docs: add code review report`）。
