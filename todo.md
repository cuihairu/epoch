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
- [x] 审核发现：Java 关闭守卫为 volatile 非锁守卫，send/poll 检查 closed 后与并发 close() 存在 TOCTOU——线程读 closed==false 后 close() 已完成，该线程仍在已关闭 adapter 上 offer/poll，可能丢消息或抛异常（已用 closeLock synchronized 严格串行 close/send/poll，java/src/main/java/io/epoch/AeronTransport.java:112-183；新增并发 send+close 测试 java/src/test/java/io/epoch/VectorTestMain.java:137-165；java run-tests.sh 绿）
- [x] 审核发现：de8fb3a 的 close() 重试语义与其新增测试均假定 adapter.close() 失败是「原子」的（FakeAeronAdapter.close() 在释放任何资源前就抛异常，adapter 保持完全可用）；但 RealAeronAdapter.close() 依次释放 publication→subscription→aeron→mediaDriver，若 subscription.close()/aeron.close() 在 publication 已关后抛异常，则 closed 仍为 false 而 publication 已关闭——后续 send() 会在已关闭 publication 上 offer（Aeron 抛 IllegalStateException，stats 不计数、非受控重试路径），poll() 同理，且运输仍对外报告「未关闭」。FakeAeronAdapter 无法复现该部分释放状态（已改为 close() 在 finally 中置 closed=true，即使 adapter.close() 抛异常也进入关闭态，后续 send/poll 受 closed 守卫阻断；java/src/main/java/io/epoch/AeronTransport.java:172-183；测试改为断言 send 抛异常、poll 空，java/src/test/java/io/epoch/VectorTestMain.java:128-137；java run-tests.sh 绿）
- [x] 审核发现：c215c87 把 stats 计数写移入 closeLock 同步块，但 AeronStats 的读取侧 stats()/sentCount()/receivedCount() 等 getter 仍未加锁——reader 线程无 happens-before，可读到陈旧或撕裂的 long 计数；本次并发正确性修复未覆盖读侧（已改 AeronStats 全字段为 AtomicLong，写用 recordSent/recordReceived/recordOfferFailure 原子累加、读用 get()，读写均无撕裂/陈旧；java/src/main/java/io/epoch/AeronTransport.java:193-251；java run-tests.sh 绿）
- [x] 审核发现：VectorTestMain 并发用例存在测试挂死风险——若 ensure(!closeStarted.await(50ms)) 断言失败，releaseOffer 永不计零、sender/closer 线程不 join、offerStarted.await() 亦无超时；非守护线程永久阻塞会使测试进程挂死而非快速失败（已改为 await()/join() 均带 5s 超时，finally 中 releaseOffer/releasePoll/releaseClose 全部 countDown；java/src/test/java/io/epoch/VectorTestMain.java:153-186、:238-252；java run-tests.sh 绿）
- [x] 审核发现：c215c87 串行化 send/poll/close 三者，但新增测试仅覆盖 send+close 竞态，poll+close 这一对称路径无对应用例（已新增并发 poll+close 用例，断言 close 等待 in-flight poll；java/src/test/java/io/epoch/VectorTestMain.java:167-186；java run-tests.sh 绿）
- [x] 审核发现：本区间 c215c87 把 AeronStats 类自 :182 下移至 :189 起，但同区间文档修复提交（75911f0/41c752e）未同步 D9/M1 的 file:line 证据「AeronTransport.java:174-224」——现引用已不指向该类实际范围（已更新 D9/M1 证据为 AeronTransport.java:189-251，docs/审计-文档一致性.md:41、:44；pnpm run docs:build 绿）
- [x] 审核发现：覆盖范围 4412eb72..HEAD 共 5 提交（c215c87、76d5ade、de8fb3a、41c752e、75911f0）全量已审；C++ 测试（76d5ade）与文档计数/总结（41c752e、75911f0）经核对自洽（三桶 10/0/2=12、P1=3/P2=9、修法 9+1+1+1=12），未另发现问题。本轮发现 5 项（Java 竞态/部分释放/测试挂死/测试缺口 + 文档行号漂移）全部修复并验证，commit cb3a205。
- [x] 审核发现：close() 在 finally 恒置 closed=true，若 adapter.close() 部分失败（RealAeronAdapter 依次关 publication→subscription→aeron→mediaDriver，java/src/main/java/io/epoch/AeronTransport.java:99-106，中途抛异常），剩余资源（aeron/mediaDriver）永久泄漏且无恢复路径——closed 恒真使后续 close() 提前返回、无法重试释放；建议拆分「关闭标记（守卫操作）」与「adapter 已成功关闭」两态以保留重试通道（已拆分 closed（守卫，恒置 true 阻止后续操作）与 released（adapter 释放成功才置 true，允许重试），close() 仅在 closed&&released 时提前返回，adapter.close() 抛异常则 released 仍 false 可重试；java/src/main/java/io/epoch/AeronTransport.java:116、:173-181；新增部分释放重试用例断言 publication 已释放仍可重试释放 subscription，java/src/test/java/io/epoch/VectorTestMain.java:142-167、:311、:376-382；java run-tests.sh 绿；commit 24fa498）
