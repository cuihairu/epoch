# Glossary

This glossary defines source-level terms used by the synchronized architecture pages: [English architecture](architecture.md) and [Chinese architecture](../../zh/guide/architecture.md). The Chinese glossary is [术语表](../../zh/guide/glossary.md).

| Term | English | Definition | Evidence |
|---|---|---|---|
| Message | Message | The seven-field portable record: epoch, channel, source, source sequence, schema, QoS, and payload. | `cpp/include/epoch/engine.h:9` |
| ActorIdParts | ActorIdParts | The decoded identity fields: region, server, process type, process index, and actor index. | `cpp/include/epoch/actor_id.h:7` |
| ActorIdCodec | ActorIdCodec | A replaceable encode/decode strategy that converts `ActorIdParts` and a 64-bit ID; Java exposes it as `ActorId.Codec` and .NET as `IActorIdCodec`. | `cpp/include/epoch/actor_id.h:15` |
| Default ActorId layout | Default ActorId layout | The default high-to-low bit split of region 10, server 12, process type 6, process index 10, and actor index 26. | `java/src/main/java/io/epoch/ActorId.java:29` |
| Epoch processing | Epoch processing | Sorting a batch by the deterministic key and folding payloads into a running state. | `java/src/main/java/io/epoch/Engine.java:56` |
| Epoch boundary | Epoch boundary | The point at which the accumulated state is emitted before processing the next distinct epoch. | `java/src/main/java/io/epoch/Engine.java:74` |
| EpochResult | EpochResult | One emitted tuple containing epoch number, accumulated state, and hexadecimal state hash. | `cpp/include/epoch/engine.h:19` |
| stateHash | stateHash | FNV-1a 64 over `state:<state>`, rendered as 16 lowercase hexadecimal characters. | `java/src/main/java/io/epoch/Engine.java:46` |
| Transport | Transport | The three-operation boundary—send, poll, and close—that isolates delivery from processing. | `cpp/include/epoch/transport.h:11` |
| InMemoryTransport | InMemoryTransport | A process-local FIFO queue used for direct message handoff; it neither sorts by epoch nor persists messages. | `java/src/main/java/io/epoch/InMemoryTransport.java:8` |
| AeronTransport | AeronTransport | A `Transport` implementation backed by Publication/Subscription; each language owns or delegates native resources explicitly. | `java/src/main/java/io/epoch/AeronTransport.java:17` |
| AeronConfig | AeronConfig | Delivery settings: channel, stream ID, Aeron directory, fragment limit, and offer retry limit; Java adds embedded-driver settings. | `cpp/include/epoch/aeron_transport.h:12` |
| AeronStats | AeronStats | Delivery counters for sends, receives, and classified offer failures. | `native/include/epoch_aeron.h:24` |
| Aeron Frame v1 | Aeron Frame v1 | The fixed 56-byte little-endian representation of one portable message. | `docs/guide/transport-aeron.md:39` |
| Native gateway | Native gateway | The opaque C object and five-function ABI that centralize Aeron C-client lifecycle for Go, Node, .NET, and Python. | `native/include/epoch_aeron.h:37` |
| Media Driver | Media Driver | The Aeron process/component used for shared memory and network IO; Java can launch it embedded while the native gateway delegates it externally. | `java/src/main/java/io/epoch/AeronTransport.java:68` |
| Canonical vector | Canonical vector | The shared `test-vectors/epoch_vector_v1.txt` input and expected epoch results used to compare implementations. | `test-vectors/epoch_vector_v1.txt:1` |
| Vector harness | Vector harness | A language-specific executable that parses the canonical file and asserts equivalent state transitions. | `java/src/test/java/io/epoch/VectorTestMain.java:172` |
| Coverage lifecycle | Coverage lifecycle | Per-language instrumentation and reporting steps that attach behavior evidence to source. | `.github/workflows/build-go.yml:36` |
| Source layer | Source layer | A responsibility boundary derived from ownership, call direction, state, and lifecycle—not a directory name. | `docs/en/guide/architecture.md:5` |
