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
- [ ] Node.js Message 64-bit 字段精度丢失，改 bigint 需设计拍板（node/src/index.ts:88-112）【BLOCKED：改 bigint 为破坏性 API 变更，需设计拍板；缺设计拍板（确认是否接受破坏性 API 变更）→ 本轮跳过，待拍板后处理；2026-11-04 复核：源码仍为 number（node/src/index.ts:88-112），docs/review/2026-11-02-code-review-opencode.md:49、:106 确认需 bigint 破坏性变更，设计拍板仍缺→保留 BLOCKED 不重试；2026-11-06 复核：node/src/index.ts:4-12、:88-93、:105-112 仍为 number/BigInt(number)/Number(bigint)，docs/review/2026-11-02-code-review-opencode.md:49、:106 结论仍适用，设计拍板仍缺→保留 BLOCKED 不重试；本轮复核（2026-11-06）：源码与上一致、全仓仍无设计拍板记录（rg 拍板/bigint 仅注记无决策）→缺设计拍板，保持未勾 BLOCKED 不重试】
- [x] Java AeronTransport 补充关闭状态守卫（volatile closed：send 后抛异常、poll 后空、close 幂等；run-tests.sh 绿）

## 审核发现
- [x] 审核发现：审计表 C1 称 architecture-en.svg「无任何页面引用」不准确——基线 README.md:5 实际引用了该资产，且其唯一引用与本删除在提交 1948936 同批移除，删除的是仍被引用的资产（已改写 docs/审计-文档一致性.md C1 与 logo 取证说明；pnpm run docs:build 绿）
- [x] 审核发现：protocol.md 列各语言 Codec 命名时遗漏 Node.js——node/src/actor-id.ts:9 同样定义 `ActorIdCodec` 类型（已补充 docs/guide/protocol.md Node.js 命名；cd node && pnpm test 绿）
- [x] 审核发现：C1 重新分类（码超前→不符）未同步更新段一三桶计数表——仍记「码超前=1 / 不符=1」，实际审计表为码超前=0、不符=2（已改为码超前=0、不符=2，docs/审计-文档一致性.md:13-14；pnpm run docs:build 绿）
- [x] 审核发现：段三总结仍称「1 项删除无引用资产」，与修正后 C1（资产被 README.md:5 引用、连同引用同批移除）口径矛盾（commit 75911f0 docs/审计-文档一致性.md:13-14；本项已改为「删除冗余资产并同步移除其唯一引用」docs/审计-文档一致性.md:25；pnpm run docs:build 绿）
- [x] 审核发现：close() 先置 closed=true 再 adapter.close()，若 adapter.close() 中途抛异常则资源仅部分释放、后续 close() 因 closed 已真而提前返回，泄漏无法重试（已改为 adapter.close() 成功后再置 closed=true，java/src/main/java/io/epoch/AeronTransport.java:165-172；新增首次 close 失败可重试用例 java/src/test/java/io/epoch/VectorTestMain.java:112-135；java run-tests.sh 绿）
- [x] 审核发现：C++ 构造器修复把 context_ 赋值提前至 context_set_dir 之前（正是为让 set_dir 失败时 close() 释放 context），但新增测试只覆盖 context_init/init/start 失败，未覆盖 context_set_dir 失败清理路径——stub_context_set_dir 恒返 0、无任何用例注入其失败（已让 stub_context_set_dir 支持注入失败，新增 test_aeron_context_set_dir_failure_cleans_up 断言 context 释放，cpp/tests/aeron_transport_test.cpp:67-80、:404-437、:466-470；ctest 3/3 绿）
- [x] 审核发现：Java 关闭守卫为 volatile 非锁守卫，send/poll 检查 closed 后与并发 close() 存在 TOCTOU——线程读 closed==false 后 close() 已完成，该线程仍在已关闭 adapter 上 offer/poll，可能丢消息或抛异常（已用 closeLock synchronized 严格串行 close/send/poll，java/src/main/java/io/epoch/AeronTransport.java:112-183；新增并发 send+close 测试 java/src/test/java/io/epoch/VectorTestMain.java:137-165；java run-tests.sh 绿）
- [x] 审核发现：de8fb3a 的 close() 重试语义与其新增测试均假定 adapter.close() 失败是「原子」的（FakeAeronAdapter.close() 在释放任何资源前就抛异常，adapter 保持完全可用）；但 RealAeronAdapter.close() 依次释放 publication→subscription→aeron→mediaDriver，若 subscription.close()/aeron.close() 在 publication 已关后抛异常，则 closed 仍为 false 而 publication 已关闭——后续 send() 会在已关闭 publication 上 offer（Aeron 抛 IllegalStateException，stats 不计数、非受控重试路径），poll() 同理，且运输仍对外报告「未关闭」。FakeAeronAdapter 无法复现该部分释放状态（已改为 close() 在 finally 中置 closed=true，即使 adapter.close() 抛异常也进入关闭态，后续 send/poll 受 closed 守卫阻断；java/src/main/java/io/epoch/AeronTransport.java:172-183；测试改为断言 send 抛异常、poll 空，java/src/test/java/io/epoch/VectorTestMain.java:128-137；java run-tests.sh 绿）
- [x] 审核发现：c215c87 把 stats 计数写移入 closeLock 同步块，但 AeronStats 的读取侧 stats()/sentCount()/receivedCount() 等 getter 仍未加锁——reader 线程无 happens-before，可读到陈旧或撕裂的 long 计数；本次并发正确性修复未覆盖读侧（已改 AeronStats 全字段为 AtomicLong，写用 recordSent/recordReceived/recordOfferFailure 原子累加、读用 get()，读写均无撕裂/陈旧；java/src/main/java/io/epoch/AeronTransport.java:193-251；java run-tests.sh 绿）
- [x] 审核发现：VectorTestMain 并发用例存在测试挂死风险——若 ensure(!closeStarted.await(50ms)) 断言失败，releaseOffer 永不计零、sender/closer 线程不 join、offerStarted.await() 亦无超时；非守护线程永久阻塞会使测试进程挂死而非快速失败（已改为 await()/join() 均带 5s 超时，finally 中 releaseOffer/releasePoll/releaseClose 全部 countDown；java/src/test/java/io/epoch/VectorTestMain.java:153-186、:238-252；java run-tests.sh 绿）
- [x] 审核发现：c215c87 串行化 send/poll/close 三者，但新增测试仅覆盖 send+close 竞态，poll+close 这一对称路径无对应用例（已新增并发 poll+close 用例，断言 close 等待 in-flight poll；java/src/test/java/io/epoch/VectorTestMain.java:167-186；java run-tests.sh 绿）
- [x] 审核发现：本区间 c215c87 把 AeronStats 类自 :182 下移至 :189 起，但同区间文档修复提交（75911f0/41c752e）未同步 D9/M1 的 file:line 证据「AeronTransport.java:174-224」——现引用已不指向该类实际范围（已更新 D9/M1 证据为 AeronTransport.java:189-251，docs/审计-文档一致性.md:41、:44；pnpm run docs:build 绿）
- [x] 审核发现：覆盖范围 4412eb72..HEAD 共 5 提交（c215c87、76d5ade、de8fb3a、41c752e、75911f0）全量已审；C++ 测试（76d5ade）与文档计数/总结（41c752e、75911f0）经核对自洽（三桶 10/0/2=12、P1=3/P2=9、修法 9+1+1+1=12），未另发现问题。本轮发现 5 项（Java 竞态/部分释放/测试挂死/测试缺口 + 文档行号漂移）全部修复并验证，commit cb3a205。
- [x] 审核发现：close() 在 finally 恒置 closed=true，若 adapter.close() 部分失败（RealAeronAdapter 依次关 publication→subscription→aeron→mediaDriver，java/src/main/java/io/epoch/AeronTransport.java:99-106，中途抛异常），剩余资源（aeron/mediaDriver）永久泄漏且无恢复路径——closed 恒真使后续 close() 提前返回、无法重试释放；建议拆分「关闭标记（守卫操作）」与「adapter 已成功关闭」两态以保留重试通道（已拆分 closed（守卫，恒置 true 阻止后续操作）与 released（adapter 释放成功才置 true，允许重试），close() 仅在 closed&&released 时提前返回，adapter.close() 抛异常则 released 仍 false 可重试；java/src/main/java/io/epoch/AeronTransport.java:116、:173-181；新增部分释放重试用例断言 publication 已释放仍可重试释放 subscription，java/src/test/java/io/epoch/VectorTestMain.java:142-167、:311、:376-382；java run-tests.sh 绿；commit 24fa498）
- [x] 审核发现：部分失败重试路径依赖 RealAeronAdapter.close() 可重复调用（对已释放的 publication/subscription/aeron/mediaDriver 再次 close 须幂等），但新增重试用例仅用 FakeAeronAdapter 验证，无针对真实 Aeron 适配器「部分失败→重试→幂等重关」的集成/幂等测试——该核心安全假设未被测试覆盖（已改为 RealAeronAdapter.close() 逐资源守卫幂等，java/src/main/java/io/epoch/AeronTransport.java:103-120；新增真实嵌入式 Aeron 适配器三连 close 幂等测试 java/src/test/java/io/epoch/VectorTestMain.java:274-282；java run-tests.sh 绿；commit fececde）
- [x] 审核发现：FakeAeronAdapter 仅在 publication 释放后模拟部分失败（failPartialClose 于 closeAttempts==1 抛异常），未覆盖 subscription/aeron/mediaDriver 阶段失败的重试路径——该情形 retry 需重关已释放的较早资源，分支无测试；且重试后 publicationsReleased 恒为 2（重关已释放 publication）但测试未断言此双释放（已改为 failAfterResources 支持 1/2/3/4 阶段注入失败，覆盖 subscription/aeron/mediaDriver 阶段并断言各资源仅释放一次；java/src/test/java/io/epoch/VectorTestMain.java:174-206、:443-470；java run-tests.sh 绿；commit fececde）
- [x] 审核发现：released 仅在 adapter.close() 正常返回时置 true；若失败发生在最末资源（如 mediaDriver.close() 抛异常，publication/subscription/aeron 已释放），实际资源已全部释放但 released 恒 false，每次 close() 都重跑整条关闭链并重抛、无法进入幂等态——语义边界属轻微问题（已通过逐资源幂等守卫解决：released 置真前重跑关闭链只重关未释放资源、已释放资源跳过，重试后进入幂等态；java/src/main/java/io/epoch/AeronTransport.java:103-120；新增 mediaDriver 阶段失败重试断言 java/src/test/java/io/epoch/VectorTestMain.java:174-206；java run-tests.sh 绿；commit fececde）
- [x] 审核发现：close() 守卫 `closed && released` 中 released 隐含 closed（released 仅在 closed 置真后的同一同步块赋值且永不回退），条件实际等价于 released，冗余 conjunct 易掩盖守卫意图（已简化为仅判 released，java/src/main/java/io/epoch/AeronTransport.java:189-191；java run-tests.sh 绿；commit fececde）
- [x] 审核发现：fececde 把 AeronStats 类自 :189-251 下移至 :206-264（新增 4 个布尔守卫字段 + synchronized close），但未同步 docs/审计-文档一致性.md:41/:44 的 D9/M1 file:line 证据「AeronTransport.java:189-251」——现引用已不指向该类实际范围，与 39 号行号漂移问题同型复发（应改为 AeronTransport.java:206-264）（已更新 D9/M1 证据为 AeronTransport.java:206-264，docs/审计-文档一致性.md:41、:44；java/src/main/java/io/epoch/AeronTransport.java:206-264；pnpm run docs:build 绿）
- [x] 审核发现：FakeAeronAdapter 的 failAfterResources 注入点与 RealAeronAdapter 各资源 close() 抛异常点错位——fake 先置「已释放」标志再抛（失败注入在资源之间），而 real 的 close() 抛异常时该资源标志恒 false、重试会重关该资源；故「首个资源 publication.close() 自身失败→重试重关」分支无覆盖（failAfterResources 1/2/3/4 实为 subscription/aeron/mediaDriver/全部已释放后失败，其中 stage=4 在 real 无对应可抛点），且 stage=4 断言 mediaDriversReleased==1 与 real 对瞬时失败重试重关 mediaDriver（实际 close 2 次）不符（已改为 failOnResource 在资源 close() 抛异常点注入（镜像 real 语义），新增每资源 close 调用计数，stage 1/2/3/4 全覆盖并断言失败资源重试重关（close 2 次）其余仅 1 次；java/src/test/java/io/epoch/VectorTestMain.java:144、:153、:166、:178-201、:388、:393、:464-489；java run-tests.sh 绿）
- [x] 审核完成 8ba4bbc..aa4d215，无新增发现；:47 所述 failOnResource 修复属该区间（git log 8ba4bbc..aa4d215 输出 aa4d215、b202c7b；pnpm run docs:build 绿）
- [x] 审核发现：本行「审核完成 8ba4bbc8eddc..aa4d215，无发现」与 :47 记录的「FakeAeronAdapter failAfterResources 注入点错位→已改 failOnResource」发现同区间矛盾——8ba4bbc..aa4d215 含 b202c7b（fix(java)，72 行 VectorTestMain.java 改动，即 :47 所述修复的落地提交），对含已记录发现及其修复提交的区间宣称「无发现」自相矛盾（属实：已改为「无新增发现」并注明 :47 属该区间；git log 8ba4bbc..aa4d215 输出 aa4d215、b202c7b；pnpm run docs:build 绿）
- [x] 审核发现：本行无复选框标记（同节其余条目均为 `- [x]`/`[x]`），且用 12 位全哈希 8ba4bbc8eddc 而文件其余均用短哈希，破坏「审核发现」节既有格式约定（属实：已改为 `- [x]` 并使用 7 位短哈希；grep -nE '^\- \[x\]' todo.md 覆盖本节所有条目；pnpm run docs:build 绿）
- [x] 审核发现：声称 grep 检查覆盖本节所有条目不成立，第 42–47 行缺少列表标记，既不会匹配该命令也不会呈现为 Markdown 复选框——属实：第 42–47 行确以 `[x]` 开头而缺 `- ` 前缀（属实：已为 42–47 行补上 `- ` 前缀，grep -nE '^\- \[x\]' todo.md 现覆盖本节全部 23 个条目标记；pnpm run docs:build 绿 18 页）
- [x] 审核发现：复审报告称基准哈希 c3c43df01be7b59c 与 test-vectors 一致，但该值对应 state:0，测试向量实际只列出 state:8 和 state:15 的哈希，无法作为所述一致性的证据（docs/审计-文档一致性.md:72）。(属实：证据行已改指测试断言 VectorTestMain.java:13、VectorTests.cs:74 与向量文件实际 E 行 state 8/15；同 commit；pnpm run docs:build 绿)
- [x] 审核发现：复审报告宣称文档与源码差异为零，但行为规范要求跨语言相同输入得到一致 stateHash，Node.js 的 Message 使用 number 会在超过安全整数范围时丢失精度，文档未说明这一已知限制（docs/审计-文档一致性.md:64）。(属实：protocol.md:25 已补现状注记——Message 字段仍为 number、超安全整数先丢精度、bigint 迁移待设计拍板（node/src/index.ts:88-112）；审计表增补 R2-1/R2-2；同 commit；pnpm run docs:build 绿)
- [x] 审核发现：R2-1「源码证据」列引用 test-vectors/epoch_vector_v1.txt:7-8 作为「E 行（state 8/15）」位置，但该文件 E 行实际在第 10-11 行（:7-8 为 M,1,1,1,1,1,3 与 M,2,1,1,2,1,7 两行），行号引用错误——R2-1 本为修正「引用口径」而设，却引入新行号漂移（属实：已改为 test-vectors/epoch_vector_v1.txt:10-11；docs/审计-文档一致性.md:92；pnpm run docs:build 绿）
- [x] 审核发现：证据行称「state:0 基准哈希 c3c43df01be7b59c 由各语言测试自证」却仅列 java/VectorTestMain.java:13 与 dotnet/VectorTests.cs:74 两处断言，cpp（epoch_vector_test.cpp:104、core_test.cpp:79）、go（epoch_test.go:93）、node（version.test.js:55）、python（test_version.py:49）亦各断言该哈希而未引用——「各语言」主张超出所给证据范围（属实：证据行已补齐 7 处断言 cpp/tests/epoch_vector_test.cpp:104、cpp/tests/core_test.cpp:79、go/epoch_test.go:93、java/src/test/java/io/epoch/VectorTestMain.java:13、node/test/version.test.js:55、python/tests/test_version.py:49、dotnet/Epoch.Tests/VectorTests.cs:74；docs/审计-文档一致性.md:72；pnpm run docs:build 绿）
- [x] 审核发现：R2-1「源码证据」列引 test-vectors/epoch_vector_v1.txt:7-8 作为「E 行（state 8/15）」位置，但该文件 E 行实际在第 10-11 行，:7-8 为 M,1,1,1,1,1,3 与 M,2,1,1,2,1,7 两行，行号引用漂移——本为修正「引用口径」而设反引入新漂移（重复/非独立发现：与 todo.md:54 为同一发现，已在 :54 按属实处置并改为 :10-11，本行不构成独立发现；证据：todo.md:54、test-vectors/epoch_vector_v1.txt:10-11；pnpm run docs:build 绿 18 页）
- [x] 审核发现：闭环称「全部页面复审无新增漂移，本轮 0 修复、0 改码」，紧随其后的「复审修订」表 R2-1/R2-2「修法」却分别述「证据行已改指测试断言与向量文件实际 E 行」「protocol.md 补现状注记」，即同节已记录 2 项修复，与「0 修复」自相矛盾（属实：闭环已限定初审门禁 0 修复、0 改码，并说明评审回执后修订表记录两项文档修复；docs/审计-文档一致性.md:84；pnpm run docs:build 绿）
- [x] 审核发现：todo.md:56 与 todo.md:54 为同一条「R2-1 引用 :7-8 而 E 行实为 :10-11」的行号漂移发现，但旧 :56（15e45fe 改写前）处置标注「不属实」、:54 却标注「属实：已改为 :10-11」，同一问题处置互相矛盾——该发现内容属实（已证实），非「不属实」，正确处置应为「重复/非独立发现」（属实：已改为「重复/非独立发现：与 :54 同一发现」并补充证据引用 todo.md:54（现 :56 证据为 todo.md:54、test-vectors/epoch_vector_v1.txt:10-11）；todo.md:56；pnpm run docs:build 绿 18 页；ctest 3/3、go test ok、python unittest 10/10、node test 11/11、java exit 0、dotnet test 8/8 全绿）
- [x] 审核发现：旧 :56（15e45fe 改写前）称「该条与上一条为同一发现」但交叉引用有误——上一条即 todo.md:55 是「各语言测试自证证据不足」的另一项独立发现，真正重复对象是 todo.md:54；且所引「证据：todo.md:54-55」混入无关条目（属实：已改为明确指向 todo.md:54、证据为 todo.md:54、test-vectors/epoch_vector_v1.txt:10-11（与 :56 现证据一致）；todo.md:56；pnpm run docs:build 绿 18 页；ctest 3/3、go test ok、python unittest 10/10、node test 11/11、java exit 0、dotnet test 8/8 全绿）
- [x] 审核发现：15e45fe 将 todo.md:56 由「不属实」改写为「重复/非独立发现」，但同 commit 新追加 :58 仍称「:56 处置标注不属实」——被引用行已改写，所指内容不复存在，自引用行号与现内容矛盾（属实：:58 已改为「旧 :56（15e45fe 改写前）处置标注「不属实」」，明确标注引用对象为改写前 :56，消除与现 :56 内容矛盾；todo.md:58；pnpm run docs:build 绿 18 页；ctest 3/3、go test ok、python unittest 10/10、node test 11/11、java exit 0、dotnet test 8/8 全绿）
- [x] 审核发现：新追加 :59 称「todo.md:56 称该条与上一条为同一发现」，但 15e45fe 已把 :56 改写为「与 todo.md:54 为同一发现」，所引原文已不存在于该行（属实：:59 已改为「旧 :56（15e45fe 改写前）称『该条与上一条为同一发现』」，明确标注引用对象，消除矛盾；todo.md:59；pnpm run docs:build 绿 18 页；ctest 3/3、go test ok、python unittest 10/10、node test 11/11、java exit 0、dotnet test 8/8 全绿）
- [x] 审核发现：:58 处置「修正证据引用为 todo.md:54」与 :59 处置「证据仅列 todo.md:54」均与实际不符——:56 现证据实为「todo.md:54、test-vectors/epoch_vector_v1.txt:10-11」，尚含向量文件引用（属实：:58 处置改为「补充证据引用 todo.md:54（现 :56 证据为 todo.md:54、test-vectors/epoch_vector_v1.txt:10-11）」、:59 处置改为「证据为 todo.md:54、test-vectors/epoch_vector_v1.txt:10-11（与 :56 现证据一致）」；todo.md:58-59；pnpm run docs:build 绿 18 页；ctest 3/3、go test ok、python unittest 10/10、node test 11/11、java exit 0、dotnet test 8/8 全绿）
- [x] 审核发现：第 58–61 行将第 56 行的改写归于「本 commit」，但本次提交 c7431ca 未修改第 56 行，该改写发生在此前的 15e45fe，导致历史证据的提交指向错误（属实：:58-:61 已改为引用 15e45fe 作为第 56 行改写提交，消除提交指向错误；todo.md:58-61；pnpm run docs:build 绿 18 页；ctest 3/3、go test ok、python unittest 10/10、node test 11/11、java exit 0、dotnet test 8/8 全绿）
- [x] 审核发现：本轮 diff 未见问题（c7431ca..a7f1fd5）。证据：git rev-list c7431ca..a7f1fd5 提交清单仅含 a7f1fd5（共 1 个），逐项复查该提交 diff 未见问题。
- [x] 审核发现：本轮 diff 未见问题（a7f1fd5..664ada7）。证据：git rev-list a7f1fd5..664ada7 提交清单仅含 664ada7（共 1 个），逐项复查该提交 diff 未见问题（仅 todo.md 追加一条勾选记录，无代码改动）。
- [x] 审核发现：本轮 diff 未见问题（664ada7..23c4885）。证据：git rev-list 664ada7..23c4885 提交清单仅含 23c4885（共 1 个），逐项复查该提交 diff 未见问题（仅 todo.md 追加一条勾选记录，无代码改动）。
- [x] 审核发现：第 66 行证据「git rev-list 664ada7..HEAD 提交清单仅含 23c4885（共 1 个）」以移动引用 HEAD 为区间端点，该行所在提交 9ad2235 本身即落入 664ada7..HEAD，现 664ada7..9ad2235 实测含 2 个提交（9ad2235、23c4885），与「共 1 个」不符，证据不可复现且自指——属实：已改第 66 行证据为固定端点 664ada7..23c4885（实测 git rev-list 664ada7..23c4885 仅含 23c4885 共 1 个，可复现且不再自指），并同批将 :64-:65 同类移动端点改为固定端点（c7431ca..a7f1fd5、a7f1fd5..664ada7）；todo.md:66；git rev-list 664ada7..23c4885 实测返回 23c4885（共 1 个）
- [x] 审核发现：第 66 行以 40 位全哈希 23c488506b8e0f9bad232fd4b91da070440f8039 引用提交，违背「审核发现」节既有 7 位短哈希约定（todo.md:50 已定该约定并明确「使用 7 位短哈希」），与 :64-:65 同类——属实：已改第 66 行哈希为 7 位 23c4885，并同批将 :64-:65 全哈希规范为 7 位（a7f1fd5、664ada7）；todo.md:66；todo.md:50
- [x] 审核发现：本 diff 将 :64-:66 证据哈希规范化时，:65 区间端点与证据哈希仍用 8 位「664ada7a」（该提交 7 位缩写实为「664ada7」），且 :68 自述「规范为 7 位（a7f1fd5、664ada7a）」所列「664ada7a」实为 8 位，与 todo.md:50 既定「使用 7 位短哈希」约定不符（todo.md:65、:68）——属实：已将 :64-:68 全部 8 位哈希规范为 7 位（c7431ca0→c7431ca、a7f1fd52→a7f1fd5、664ada7a→664ada7），并同步更新 :67-:68 相应描述；todo.md:64-68；todo.md:50；git rev-parse --short=7 实测 c7431ca0→c7431ca、a7f1fd52→a7f1fd5、664ada7a→664ada7
- [x] 审核发现：:67 修复陈述「现 HEAD 实测含 2 个提交（9ad2235、23c4885）」仍以移动引用 HEAD 为基准——该行所在提交 a60dd3d 亦落入 664ada7..HEAD，现 664ada7..a60dd3d 实测含 3 个提交（a60dd3d、9ad2235、23c4885），与「含 2 个」不符，证据不可复现且自指，与本行所述 :66 旧证据问题同型复发（todo.md:67）——属实：已改 :67 该陈述为固定端点「现 664ada7..9ad2235 实测含 2 个提交（9ad2235、23c4885）」，实测 git rev-list 664ada7..9ad2235 返回 9ad2235、23c4885（共 2 个），可复现且不再自指；todo.md:67
- [x] 审核发现：本轮 diff 未见问题（a60dd3d..62c1bb6）。证据：git rev-list a60dd3d..62c1bb6 提交清单仅含 62c1bb6（共 1 个），逐项复查该提交 diff 未见问题（仅 todo.md 追加勾选记录，无代码改动）；区间端点 a60dd3d..62c1bb6 为 7 位固定端点，符合 todo.md:50 约定。
- [x] 审核发现：本轮 diff 未见问题（62c1bb6..6a8b6d8）。证据：git rev-list 62c1bb6..6a8b6d8 提交清单仅含 6a8b6d8（共 1 个），逐项复查该提交 diff 未见问题（仅 todo.md：为 Node bigint BLOCKED 项追加 2026-11-06 复核注记并新增上一条审计记录，无代码改动）；区间端点 62c1bb6..6a8b6d8 为 7 位固定端点，符合 todo.md:50 约定；pnpm run docs:build 绿 18 页；ctest 3/3、go test ok、python unittest 10/10、node test 11/11、java exit 0、dotnet test 8/8（DOTNET_ROLL_FORWARD=Major）全绿
- [x] 审核发现：本轮 diff（6a8b6d8..HEAD，仅 a511f84 纯新增 :72）自述「原行区间端点 62c1bb6a 为 8 位且 HEAD 为移动引用…改为 7 位固定端点 62c1bb6..6a8b6d8」，但该行本为纯新增（git show a511f84 仅 +1 行），父提交 6a8b6d8 的 todo.md 止于 :71 且全文件无「62c1bb6a」（git grep 实测 0 处），所叙「原行…改为」改动不存在于本 diff、不可从 git 历史复现，属 :66-:70 已定「证据不可复现/自指」缺陷同型——属实：已改 :72（并同批 :71 同类「原行…改为」伪述）为纯新增行的真实证据（无原行可改），todo.md:71-72；git show a511f84 仅 +1 行、git grep -n 62c1bb6a 6a8b6d8 实测 0 处；pnpm run docs:build 绿 18 页；ctest 3/3、go test ok、python unittest 10/10、node test 11/11、java exit 0、dotnet test 8/8（DOTNET_ROLL_FORWARD=Major）全绿
- [ ] 审核发现：以 HEAD 为终点的 6a8b6d8..HEAD 区间现包含后续提交，故该区间「仅 a511f84」的描述不可复现（todo.md:73）

## 源码分层分析 2026-11-07

### 抽象层清单

- L1 协议与身份：统一定义七字段消息、ActorId 位布局和可替换 codec——代表 `cpp/include/epoch/engine.h:9`、`cpp/include/epoch/actor_id.h:15`。
- L2 确定性 Epoch 处理：按固定五元组排序并折叠 payload，输出 `EpochResult` 和 FNV stateHash——代表 `java/src/main/java/io/epoch/Engine.java:56`。
- L3 传输边界与本地队列：约束 send/poll/close 生命周期；本地 FIFO 只留在进程内存——代表 `cpp/include/epoch/transport.h:11`。
- L4 Aeron 投递适配器：把传输契约桥接到 Aeron publication/subscription、重试、fragment 解码和帧编解码——代表 `cpp/src/aeron_transport.cpp:223`。
- L5 原生 Aeron 网关：用不透明 C 对象和五函数 ABI 集中管理 Aeron C client 状态——代表 `native/include/epoch_aeron.h:37`。
- L6 跨语言验证与打包：共享向量、各语言 harness、coverage、native/语言制品构建与发布生命周期——代表 `test-vectors/epoch_vector_v1.txt:1`、`.github/workflows/nightly.yml:18`。

### 超大文件候选

无 >1000 行文件（tracked 源码最大文件为 `java/src/test/java/io/epoch/VectorTestMain.java`，516 行）。

### 目录/分类问题

- 未见分类问题。`cpp/include`/`cpp/src` 与各语言包名只承载实现入口；实际跨语言契约由共享头/头文件、协议文档和测试向量约束。
