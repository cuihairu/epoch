# Source Layer Architecture

This page records the runtime responsibilities that exist in source code, not the repository directory names. The Chinese version is synchronized at [架构目录解说](../../zh/guide/architecture.md); terminology is defined in [Glossary](glossary.md).

## Layer Model

| Layer | Responsibility |
|---|---|
| L1 Protocol and Identity | Define the portable message fields, ActorId layout, and replaceable identity coding rules. |
| L2 Deterministic Epoch Processing | Sort an input batch, fold payloads, and emit per-Epoch state and state hashes. |
| L3 Transport Boundary and Local Queue | Provide `send`/`poll`/`close` and keep local message ownership explicit. |
| L4 Aeron Delivery Adapter | Bridge the transport boundary to Publication/Subscription lifecycle and frame codec. |
| L5 Native Aeron Gateway | Own Aeron C client state behind one small C ABI. |
| L6 Cross-Language Verification and Packaging | Keep shared vectors, per-language test/build paths, coverage, and artifact boundaries executable. |

## L1 — Protocol and Identity

Each runtime exposes seven fixed `Message` fields. C++ declares them in `cpp/include/epoch/engine.h:9`; Java in `java/src/main/java/io/epoch/Engine.java:14`; .NET in `dotnet/Engine.cs:5`; Go in `go/epoch.go:10`; Node in `node/src/index.ts:4`; and Python in `python/epoch/engine.py:11`. `ActorIdParts` and the codec extension points are represented by `cpp/include/epoch/actor_id.h:7` and `cpp/include/epoch/actor_id.h:15`, `java/src/main/java/io/epoch/ActorId.java:7` and `java/src/main/java/io/epoch/ActorId.java:10`, `dotnet/ActorId.cs:6` and `dotnet/ActorId.cs:19`, `go/actor_id.go:5` and `go/actor_id.go:14`, `node/src/actor-id.ts:1` and `node/src/actor-id.ts:9`, and `python/epoch/actor_id.py:11` and `python/epoch/actor_id.py:20`. Every default codec enforces the same 10/12/6/10/26-bit split—for example, Java at `java/src/main/java/io/epoch/ActorId.java:29` and Node at `node/src/actor-id.ts:15`.

There is no separate machine-readable schema file. The cross-language contract is prose in `docs/guide/protocol.md:10` plus concrete per-language definitions and tests. Node deliberately keeps `Message` as `number` (`node/src/index.ts:4`) while its ActorId codec uses `BigInt` (`node/src/actor-id.ts:11`); the precision boundary is therefore an L1 API constraint, already recorded in `docs/guide/protocol.md:25`.

## L2 — Deterministic Epoch Processing

The portable operation is a batch function, not a persistent actor runtime. It sorts by `(epoch, channelId, qos descending, sourceId, sourceSeq)` and folds payloads into a running state: Java at `java/src/main/java/io/epoch/Engine.java:56`, .NET at `dotnet/Engine.cs:58`, Go at `go/epoch.go:44`, Node at `node/src/index.ts:40`, and Python at `python/epoch/engine.py:37`. C++ exposes the same contract in `cpp/include/epoch/engine.h:25` and implements ordering at `cpp/src/engine.cpp:14`.

The implementation owns only function-local state—the ordered input, current epoch, running state, and result list; Java shows these at `java/src/main/java/io/epoch/Engine.java:64`, Go at `go/epoch.go:50`, Node at `node/src/index.ts:51`, and Python at `python/epoch/engine.py:42`. There is no Redis, MySQL, or file-backed runtime state. Every epoch boundary emits `state:<state>` hashed with FNV-1a 64; Java shows the hash call at `java/src/main/java/io/epoch/Engine.java:75`, .NET at `dotnet/Engine.cs:87`, Node at `node/src/index.ts:63`, and Python at `python/epoch/engine.py:54`. The behavior contract lives in `docs/guide/behavior.md:15` and `docs/guide/behavior.md:27`.

## L3 — Transport Boundary and Local Queue

The transport contract is intentionally three operations. It is `Transport` in `cpp/include/epoch/transport.h:11`, `io.epoch.Transport` in `java/src/main/java/io/epoch/Transport.java:5`, `ITransport` in `dotnet/Transport.cs:3`, `Transport` in `go/transport.go:4`, `Transport` in `python/epoch/transport.py:9`, and the duck-typed `InMemoryTransport`/`AeronTransport` API surface in `node/src/index.ts:115` and `node/src/index.ts:135`.

`InMemoryTransport` is not an epoch queue; it is FIFO and does not sort or coalesce by epoch. Each implementation stores messages only in process memory: C++ deque at `cpp/include/epoch/transport.h:43`, Java deque at `java/src/main/java/io/epoch/InMemoryTransport.java:9`, .NET queue at `dotnet/Transport.cs:12`, Go slice at `go/transport.go:11`, Node array at `node/src/index.ts:116`, and Python deque at `python/epoch/transport.py:22`. `close()` clears that queue rather than making later sends/polls unavailable; see `java/src/main/java/io/epoch/InMemoryTransport.java:26` and `cpp/include/epoch/transport.h:37`.

## L4 — Aeron Delivery Adapter

C++ is the direct Aeron C client adapter: `AeronConfig` and stats are declared at `cpp/include/epoch/aeron_transport.h:12` and `cpp/include/epoch/aeron_transport.h:20`; context/client/publication/subscription state at `cpp/include/epoch/aeron_transport.h:49`; offer retry logic at `cpp/src/aeron_transport.cpp:223`; fragment drain at `cpp/src/aeron_transport.cpp:284`; and ordered publication/subscription/client/context shutdown at `cpp/src/aeron_transport.cpp:316`.

Java is intentionally independent and can own an embedded Media Driver: configuration defaults are at `java/src/main/java/io/epoch/AeronTransport.java:18`, driver/client/publication/subscription construction at `java/src/main/java/io/epoch/AeronTransport.java:68`, and poll/receive behavior at `java/src/main/java/io/epoch/AeronTransport.java:188`. The other managed runtimes call the native gateway rather than duplicate the Aeron lifecycle: Go opens the C handle at `go/aeron_transport.go:144`; Node loads Koffi bindings at `node/src/aeron-native.ts:27` and opens a handle at `node/src/index.ts:140`; .NET uses P/Invoke declarations at `dotnet/AeronTransport.cs:240`; Python loads ctypes at `python/epoch/aeron_transport.py:127` and opens a handle at `python/epoch/aeron_transport.py:195`.

All adapters map the portable message to Aeron Frame v1 before crossing the boundary. Native, C++, Java, Go, Node, and Python encoders/decoders are anchored at `native/src/epoch_aeron.c:333`, `cpp/src/aeron_transport.cpp:127`, `java/src/main/java/io/epoch/AeronMessageCodec.java:22`, `go/aeron_transport.go:113`, `node/src/index.ts:84`, and `python/epoch/aeron_transport.py:41`. The wire layout is documented in `docs/guide/transport-aeron.md:39`.

## L5 — Native Aeron Gateway

`native/src/epoch_aeron.c` owns one opaque `epoch_aeron_transport` object. Its lifecycle state consists of config, stats, closed flag, Aeron context/client, publication, subscription, and the receive buffer—declared at `native/src/epoch_aeron.c:10`. `epoch_aeron_open` builds that state through initialization, start, asynchronous publication/subscription setup, and completion polling at `native/src/epoch_aeron.c:130`; failures unwind through `epoch_aeron_close` at `native/src/epoch_aeron.c:152`. It does not own an embedded Media Driver or durable storage; Aeron's configured directory remains outside this object.

The ABI schema is exactly `epoch_aeron_config_t` and `epoch_aeron_stats_t` in `native/include/epoch_aeron.h:14` and `native/include/epoch_aeron.h:24`, plus five functions in `native/include/epoch_aeron.h:37`. This header is the exclusive protocol boundary used by Go, Node, .NET, and Python; frame framing is also exposed as `EPOCH_AERON_FRAME_LENGTH` at `native/include/epoch_aeron.h:10`.

## L6 — Cross-Language Verification and Packaging

The canonical vector is `test-vectors/epoch_vector_v1.txt:1`; its three record types and meanings are defined in `docs/guide/protocol.md:42`. C++ builds the library and three test executables at `cpp/CMakeLists.txt:29`; Java's executable vector harness compiles main and test sources at `java/scripts/run-tests.sh:11`; Go and Python bind the shared file at `go/epoch_test.go:207` and `python/tests/test_version.py:42`; Node's tests exercise version, codec, engine, in-memory transport, and native-frame behavior through `node/package.json:10`.

Configuration and lifecycle inputs are split by artifact. Native builds the Aeron submodule and shared library at `native/CMakeLists.txt:4` and `native/CMakeLists.txt:39`; C++ links the same submodule statically at `cpp/CMakeLists.txt:19`; Java downloads pinned client/driver/Agrona jars into its cache at `java/scripts/fetch-aeron.sh:7`; Go requires native build output through `LD_LIBRARY_PATH` in `.github/workflows/build-go.yml:29`; Node loads Koffi and resolves `native/build` at `node/src/aeron-native.ts:49` and `node/src/aeron-native.ts:152`; Python prefers `EPOCH_AERON_LIBRARY`, then the repository path, then system discovery at `python/epoch/aeron_transport.py:230`; .NET relies on the platform loader at `dotnet/AeronTransport.cs:242`. CI runs the per-language workflows from `.github/workflows/build-cpp.yml:1` through `.github/workflows/build-python.yml:1`, while `nightly.yml` turns each tested artifact into a package from `native` through Python at `.github/workflows/nightly.yml:18`.

## Why These Layers Are Split

1. **Portable semantics are separated from memory layout.** The same logical ordering and fold algorithm appears independently in Java (`java/src/main/java/io/epoch/Engine.java:56`), .NET (`dotnet/Engine.cs:58`), Go (`go/epoch.go:44`), Node (`node/src/index.ts:40`), and Python (`python/epoch/engine.py:37`); shared vectors can detect divergence without forcing one runtime ABI.
2. **Identity parsing is isolated because it is replaceable and precision-sensitive.** Each language defines a codec interface and a default implementation—C++ at `cpp/include/epoch/actor_id.h:15`, Java at `java/src/main/java/io/epoch/ActorId.java:10`, .NET at `dotnet/ActorId.cs:19`, Go at `go/actor_id.go:14`, Node at `node/src/actor-id.ts:9`, Python at `python/epoch/actor_id.py:20`; Node also isolates the `BigInt` codec from its `number` message API at `node/src/actor-id.ts:11` and `node/src/index.ts:4`.
3. **Transport is an explicit seam so deterministic processing never depends on media state.** C++ headers import only `epoch/engine.h` into `Transport` at `cpp/include/epoch/transport.h:3`; Python's `Transport` imports only `Message` at `python/epoch/transport.py:6`; Aeron adapters implement the same three methods without exposing Publication/Subscription types upward.
4. **The local queue and network adapter have different ownership and close semantics.** In-memory storage is a plain process-local FIFO (`java/src/main/java/io/epoch/InMemoryTransport.java:9`); Aeron closes client-owned publication, subscription, client, and driver resources (`java/src/main/java/io/epoch/AeronTransport.java:50`), so one fake abstraction would hide resource and failure behavior.
5. **Aeron adapter duplication is intentional at the ownership boundary.** C++ directly owns native pointers (`cpp/include/epoch/aeron_transport.h:49`) and Java owns an embedded driver (`java/src/main/java/io/epoch/AeronTransport.java:68`), while Go/Node/.NET/Python delegate opaque state to `epoch_aeron` (`native/include/epoch_aeron.h:12`). This prevents embedding C-client lifecycle management into four additional managed runtimes.
6. **The C ABI is kept to five operations and two structs.** `native/include/epoch_aeron.h:37` is the complete callable surface; Node declares exactly those bindings at `node/src/aeron-native.ts:49`, Python at `python/epoch/aeron_transport.py:127`, Go at `go/aeron_transport.go:65`, and .NET at `dotnet/AeronTransport.cs:240`.
7. **Wire codec is coupled to delivery, not to core processing.** `processMessages` consumes already-decoded portable messages and has no frame references (`cpp/include/epoch/engine.h:25`, `python/epoch/engine.py:37`), while each Aeron path owns encode/decode at the adapters listed above and `native/src/epoch_aeron.c:333`.
8. **Verification is a first-class layer because behavior equivalence is the product contract.** The one canonical vector is parsed independently by Java (`java/src/test/java/io/epoch/VectorTestMain.java:172`), Go (`go/epoch_test.go:207`), Python (`python/tests/test_version.py:42`), and C++ tests configured at `cpp/CMakeLists.txt:42`; this catches language-specific sort/hash/integer drift that source reading cannot guarantee.
9. **Packaging lifecycle must remain outside runtime objects.** Native artifact creation copies only built libraries plus `epoch_aeron.h` at `.github/workflows/nightly.yml:44`; Java pins jars and emits classpaths at `java/scripts/fetch-aeron.sh:7`; library discovery is deliberately separated from `AeronTransport` state in Python (`python/epoch/aeron_transport.py:230`) and Node (`node/src/aeron-native.ts:152`).
10. **Test hooks are scoped to the adapter, not promoted to public transport policy.** C++ exposes `AeronHooks` under `detail` at `cpp/include/epoch/aeron_transport.h:58`; Go replaces C call closures at `go/aeron_transport.go:65`; Node and Python inject fake native bindings at `node/src/index.ts:140` and `python/epoch/aeron_transport.py:195`. This keeps failure injection adjacent to the external dependency that owns it.

## Documentation Placement

Protocol prose remains in the existing Chinese-only document system: `docs/guide/protocol.md` owns field/ActorId/frame references, `docs/guide/behavior.md` owns ordering and hash rules, and `docs/guide/transport-aeron.md` owns Aeron lifecycle/configuration. The site is configured by `docs/.vuepress/config.js:1`; this page is reachable through explicit links from `docs/README.md` only when a future navigation entry is added, and it currently does not alter site configuration. Runtime code contains no user-facing copy strings to localize.
