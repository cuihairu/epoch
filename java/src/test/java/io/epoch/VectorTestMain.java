package io.epoch;

import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.atomic.AtomicBoolean;

public final class VectorTestMain {
    public static void main(String[] args) throws Exception {
        ensure("0.1.0".equals(Version.value()), "Version mismatch");
        ensure("c3c43df01be7b59c".equals(Engine.fnv1a64Hex("state:0")), "Hash mismatch");
        ensure("a430d84680aabd0b".equals(Engine.fnv1a64Hex("hello")), "Hash mismatch");
        ensure(Engine.processMessages(new ArrayList<>()).isEmpty(), "Empty process failed");
        ActorId.Parts actorParts = new ActorId.Parts(1, 2, 3, 4, 5);
        long actorId = ActorId.encode(actorParts);
        ActorId.Parts decoded = ActorId.decode(actorId);
        ensure(actorParts.equals(decoded), "ActorId codec mismatch");
        ensure("default".equals(ActorId.DEFAULT.name()), "ActorId codec name mismatch");
        boolean actorIdFailed = false;
        try {
            ActorId.encode(new ActorId.Parts(1 << 10, 1, 1, 1, 1));
        } catch (IllegalArgumentException ex) {
            actorIdFailed = true;
        }
        ensure(actorIdFailed, "ActorId out of range mismatch");

        AeronTransport.AeronConfig normalized = new AeronTransport.AeronConfig(
            "aeron:ipc", 1, "", 0, 0, null, false, false, false);
        ensure(normalized.fragmentLimit() == 64, "AeronConfig fragment limit mismatch");
        ensure(normalized.offerMaxAttempts() == 10, "AeronConfig offer attempts mismatch");
        ensure(normalized.idleStrategy() != null, "AeronConfig idle strategy mismatch");

        AeronTransport.AeronConfig defaultsConfig = AeronTransport.AeronConfig.defaults("aeron:ipc", 2, "");
        ensure(defaultsConfig.fragmentLimit() == 64, "AeronConfig.defaults fragment limit mismatch");
        ensure(defaultsConfig.offerMaxAttempts() == 10, "AeronConfig.defaults offer attempts mismatch");
        ensure(!defaultsConfig.embeddedDriver(), "AeronConfig.defaults should not embed driver");
        ensure(!defaultsConfig.dirDeleteOnStart(), "AeronConfig.defaults dirDeleteOnStart mismatch");
        ensure(!defaultsConfig.dirDeleteOnShutdown(), "AeronConfig.defaults dirDeleteOnShutdown mismatch");

        AeronTransport.AeronConfig embeddedConfig =
            AeronTransport.AeronConfig.embeddedDefaults("aeron:ipc", 2, "");
        ensure(embeddedConfig.embeddedDriver(), "AeronConfig.embeddedDefaults should embed driver");
        ensure(embeddedConfig.dirDeleteOnStart(), "AeronConfig.embeddedDefaults dirDeleteOnStart mismatch");
        ensure(embeddedConfig.dirDeleteOnShutdown(), "AeronConfig.embeddedDefaults dirDeleteOnShutdown mismatch");

        List<Engine.Message> sampleMessages = new ArrayList<>();
        sampleMessages.add(new Engine.Message(2, 2, 1, 2, 100, 0, 5));
        sampleMessages.add(new Engine.Message(1, 1, 2, 1, 100, 0, 2));
        sampleMessages.add(new Engine.Message(1, 1, 1, 2, 100, 0, -1));
        sampleMessages.add(new Engine.Message(3, 1, 1, 1, 100, 0, 4));
        List<Engine.EpochResult> sampleResults = Engine.processMessages(sampleMessages);
        ensure(sampleResults.size() == 3, "Sample result count mismatch");
        ensure(sampleResults.get(0).epoch == 1 && sampleResults.get(0).state == 1 &&
            "c3c43ef01be7b74f".equals(sampleResults.get(0).hash), "Sample epoch 1 mismatch");
        ensure(sampleResults.get(1).epoch == 2 && sampleResults.get(1).state == 6 &&
            "c3c43bf01be7b236".equals(sampleResults.get(1).hash), "Sample epoch 2 mismatch");
        ensure(sampleResults.get(2).epoch == 3 && sampleResults.get(2).state == 10 &&
            "8e2e70ff6abccccd".equals(sampleResults.get(2).hash), "Sample epoch 3 mismatch");

        InMemoryTransport transport = new InMemoryTransport();
        transport.send(new Engine.Message(1, 1, 1, 1, 100, 0, 1));
        transport.send(new Engine.Message(1, 1, 1, 2, 100, 0, 2));
        transport.send(new Engine.Message(2, 1, 1, 3, 100, 0, 3));
        ensure(transport.poll(0).isEmpty(), "Transport poll(0) should be empty");
        List<Engine.Message> first = transport.poll(2);
        ensure(first.size() == 2 && first.get(0).payload == 1 && first.get(1).payload == 2,
            "Transport first poll mismatch");
        List<Engine.Message> second = transport.poll(5);
        ensure(second.size() == 1 && second.get(0).payload == 3, "Transport second poll mismatch");
        transport.close();
        ensure(transport.poll(1).isEmpty(), "Transport close mismatch");

        org.agrona.concurrent.UnsafeBuffer buffer =
            new org.agrona.concurrent.UnsafeBuffer(new org.agrona.ExpandableArrayBuffer(AeronMessageCodec.FRAME_LENGTH));
        Engine.Message codecMessage = new Engine.Message(9, 8, 7, 6, 5, 4, -3);
        AeronMessageCodec.encode(buffer, 0, codecMessage);
        Engine.Message decodedMessage = AeronMessageCodec.decode(buffer, 0);
        ensure(decodedMessage.epoch == codecMessage.epoch &&
               decodedMessage.channelId == codecMessage.channelId &&
               decodedMessage.sourceId == codecMessage.sourceId &&
               decodedMessage.sourceSeq == codecMessage.sourceSeq &&
               decodedMessage.schemaId == codecMessage.schemaId &&
               decodedMessage.qos == codecMessage.qos &&
               decodedMessage.payload == codecMessage.payload, "AeronMessageCodec mismatch");
        buffer.putByte(0, (byte) 2);
        boolean codecFailed = false;
        try {
            AeronMessageCodec.decode(buffer, 0);
        } catch (IllegalArgumentException ex) {
            codecFailed = true;
        }
        ensure(codecFailed, "AeronMessageCodec version mismatch");

        AeronTransport.AeronConfig aeronConfig = new AeronTransport.AeronConfig(
            "aeron:ipc", 42, "", 64, 10, new org.agrona.concurrent.BusySpinIdleStrategy(), false, false, false);
        AeronTransport aeronTransport = new AeronTransport(aeronConfig, new FakeAeronAdapter());
        try {
            List<Engine.Message> aeronOut = List.of();
            Engine.Message aeronMessage = new Engine.Message(1, 1, 1, 1, 1, 1, 5);
            aeronTransport.send(aeronMessage);
            aeronOut = aeronTransport.poll(1);
            ensure(!aeronOut.isEmpty(), "AeronTransport poll empty");
            ensure(aeronOut.get(0).payload == 5, "AeronTransport payload mismatch");
            ensure(aeronTransport.stats().sentCount() >= 1, "AeronTransport stats sent mismatch");
            ensure(aeronTransport.stats().receivedCount() >= 1, "AeronTransport stats received mismatch");
            ensure(aeronTransport.poll(0).isEmpty(), "AeronTransport poll(0) mismatch");
        } finally {
            aeronTransport.close();
        }

        // Closed transport guard: send throws, poll returns empty, close is idempotent.
        AeronTransport.AeronConfig closedConfig = new AeronTransport.AeronConfig(
            "aeron:ipc", 43, "", 64, 10, new org.agrona.concurrent.BusySpinIdleStrategy(), false, false, false);
        AeronTransport closedTransport = new AeronTransport(closedConfig, new FakeAeronAdapter());
        closedTransport.close();
        boolean sendAfterCloseFailed = false;
        try {
            closedTransport.send(new Engine.Message(1, 1, 1, 1, 1, 1, 1));
        } catch (IllegalStateException ex) {
            sendAfterCloseFailed = true;
        }
        ensure(sendAfterCloseFailed, "AeronTransport send after close mismatch");
        ensure(closedTransport.poll(1).isEmpty(), "AeronTransport poll after close mismatch");
        closedTransport.close();

        AeronTransport.AeronConfig retryCloseConfig = new AeronTransport.AeronConfig(
            "aeron:ipc", 44, "", 64, 10, new org.agrona.concurrent.BusySpinIdleStrategy(), false, false, false);
        FakeAeronAdapter retryCloseAdapter = new FakeAeronAdapter();
        retryCloseAdapter.failFirstClose = true;
        AeronTransport retryCloseTransport = new AeronTransport(retryCloseConfig, retryCloseAdapter);
        boolean closeRetryFailed = false;
        try {
            retryCloseTransport.close();
        } catch (IllegalStateException ex) {
            closeRetryFailed = true;
        }
        ensure(closeRetryFailed, "AeronTransport first close should fail");
        ensure(retryCloseAdapter.closeAttempts == 1, "AeronTransport failed close attempts mismatch");
        boolean sendAfterFailedCloseBlocked = false;
        try {
            retryCloseTransport.send(new Engine.Message(1, 1, 1, 1, 1, 1, 7));
        } catch (IllegalStateException ex) {
            sendAfterFailedCloseBlocked = true;
        }
        ensure(sendAfterFailedCloseBlocked, "AeronTransport should close state after failed close");
        ensure(retryCloseTransport.poll(1).isEmpty(), "AeronTransport should poll empty after failed close");
        retryCloseTransport.close();
        ensure(retryCloseAdapter.closeAttempts == 2, "AeronTransport close retry attempts mismatch");
        retryCloseTransport.close();
        ensure(retryCloseAdapter.closeAttempts == 2, "AeronTransport close should be idempotent after success");

        // Partial close failure: adapter releases some resources then throws; transport must
        // keep operations blocked yet allow a retry to release the remaining resources.
        FakeAeronAdapter partialCloseAdapter = new FakeAeronAdapter();
        partialCloseAdapter.failOnResource = 1;
        AeronTransport partialCloseTransport = new AeronTransport(retryCloseConfig, partialCloseAdapter);
        boolean partialCloseFailed = false;
        try {
            partialCloseTransport.close();
        } catch (IllegalStateException ex) {
            partialCloseFailed = true;
        }
        ensure(partialCloseFailed, "AeronTransport partial close should fail");
        ensure(partialCloseAdapter.publicationCloseCalls == 1, "AeronTransport partial close should attempt publication once");
        ensure(partialCloseAdapter.publicationsReleased == 0, "AeronTransport partial release should not release publication on failed close");
        ensure(partialCloseAdapter.subscriptionsReleased == 0, "AeronTransport partial release should not release subscription");
        boolean sendAfterPartialCloseBlocked = false;
        try {
            partialCloseTransport.send(new Engine.Message(1, 1, 1, 1, 1, 1, 7));
        } catch (IllegalStateException ex) {
            sendAfterPartialCloseBlocked = true;
        }
        ensure(sendAfterPartialCloseBlocked, "AeronTransport should block send after partial close");
        ensure(partialCloseTransport.poll(1).isEmpty(), "AeronTransport should poll empty after partial close");
        partialCloseTransport.close();
        ensure(partialCloseAdapter.closeAttempts == 2, "AeronTransport should retry partial close");
        ensure(partialCloseAdapter.publicationCloseCalls == 2, "AeronTransport partial retry should re-attempt publication close");
        ensure(partialCloseAdapter.publicationsReleased == 1, "AeronTransport partial retry should release publication once");
        ensure(partialCloseAdapter.subscriptionsReleased == 1, "AeronTransport partial retry should release remaining");
        ensure(partialCloseAdapter.aeronsReleased == 1, "AeronTransport partial retry should release aeron");
        ensure(partialCloseAdapter.mediaDriversReleased == 1, "AeronTransport partial retry should release media driver");
        partialCloseTransport.close();
        ensure(partialCloseAdapter.closeAttempts == 2, "AeronTransport close idempotent after partial retry");

        // Partial close failures at publication, subscription, Aeron client, and media-driver
        // stages retry only the resource whose close() threw; already-released resources are not
        // re-closed. This mirrors RealAeronAdapter.close(), which throws at a resource's close()
        // while its flag is still false (so retry re-attempts that resource).
        int[] partialCloseStages = {1, 2, 3, 4};
        int[] stagePublicationCalls = {2, 1, 1, 1};
        int[] stageSubscriptionCalls = {1, 2, 1, 1};
        int[] stageAeronCalls = {1, 1, 2, 1};
        int[] stageMediaDriverCalls = {1, 1, 1, 2};
        int[] stagePublications = {1, 1, 1, 1};
        int[] stageSubscriptions = {1, 1, 1, 1};
        int[] stageAerons = {1, 1, 1, 1};
        int[] stageMediaDrivers = {1, 1, 1, 1};
        for (int stageIndex = 0; stageIndex < partialCloseStages.length; stageIndex++) {
            FakeAeronAdapter stagedAdapter = new FakeAeronAdapter();
            stagedAdapter.failOnResource = partialCloseStages[stageIndex];
            AeronTransport stagedTransport = new AeronTransport(retryCloseConfig, stagedAdapter);
            boolean stagedCloseFailed = false;
            try {
                stagedTransport.close();
            } catch (IllegalStateException ex) {
                stagedCloseFailed = true;
            }
            ensure(stagedCloseFailed, "AeronTransport staged close should fail");
            stagedTransport.close();
            ensure(stagedAdapter.closeAttempts == 2, "AeronTransport staged close should retry once");
            ensure(stagedAdapter.publicationCloseCalls == stagePublicationCalls[stageIndex],
                "AeronTransport staged publication close call mismatch");
            ensure(stagedAdapter.subscriptionCloseCalls == stageSubscriptionCalls[stageIndex],
                "AeronTransport staged subscription close call mismatch");
            ensure(stagedAdapter.aeronCloseCalls == stageAeronCalls[stageIndex],
                "AeronTransport staged aeron close call mismatch");
            ensure(stagedAdapter.mediaDriverCloseCalls == stageMediaDriverCalls[stageIndex],
                "AeronTransport staged media driver close call mismatch");
            ensure(stagedAdapter.publicationsReleased == stagePublications[stageIndex],
                "AeronTransport staged publication release mismatch");
            ensure(stagedAdapter.subscriptionsReleased == stageSubscriptions[stageIndex],
                "AeronTransport staged subscription release mismatch");
            ensure(stagedAdapter.aeronsReleased == stageAerons[stageIndex],
                "AeronTransport staged aeron release mismatch");
            ensure(stagedAdapter.mediaDriversReleased == stageMediaDrivers[stageIndex],
                "AeronTransport staged media driver release mismatch");
            stagedTransport.close();
            ensure(stagedAdapter.closeAttempts == 2, "AeronTransport staged close should be idempotent");
        }

        // Concurrent send + close: close must wait for in-flight send to finish.
        AeronTransport.AeronConfig raceConfig = new AeronTransport.AeronConfig(
            "aeron:ipc", 45, "", 64, 10, new org.agrona.concurrent.BusySpinIdleStrategy(), false, false, false);
        FakeAeronAdapter sendRaceAdapter = new FakeAeronAdapter();
        sendRaceAdapter.blockOffer.set(true);
        sendRaceAdapter.blockClose.set(true);
        AeronTransport sendRaceTransport = new AeronTransport(raceConfig, sendRaceAdapter);
        Thread sender = new Thread(() -> {
            try {
                sendRaceTransport.send(new Engine.Message(1, 1, 1, 1, 1, 1, 1));
            } catch (RuntimeException ignored) {
            }
        });
        sender.start();
        await(sendRaceAdapter.offerStarted, 5000, "send should start");
        Thread sendCloser = new Thread(sendRaceTransport::close);
        sendCloser.start();
        try {
            ensure(!sendRaceAdapter.closeStarted.await(50, java.util.concurrent.TimeUnit.MILLISECONDS),
                "AeronTransport close should wait for in-flight send");
        } finally {
            sendRaceAdapter.releaseOffer.countDown();
            sendRaceAdapter.releaseClose.countDown();
        }
        join(sender, "send thread should finish");
        join(sendCloser, "close thread should finish");
        ensure(sendRaceAdapter.closeAttempts == 1, "AeronTransport close after concurrent send mismatch");

        // Concurrent poll + close: close must wait for in-flight poll to finish.
        FakeAeronAdapter pollRaceAdapter = new FakeAeronAdapter();
        pollRaceAdapter.blockPoll.set(true);
        pollRaceAdapter.blockClose.set(true);
        AeronTransport pollRaceTransport = new AeronTransport(raceConfig, pollRaceAdapter);
        Thread poller = new Thread(() -> pollRaceTransport.poll(1));
        poller.start();
        await(pollRaceAdapter.pollStarted, 5000, "poll should start");
        Thread pollCloser = new Thread(pollRaceTransport::close);
        pollCloser.start();
        try {
            ensure(!pollRaceAdapter.closeStarted.await(50, java.util.concurrent.TimeUnit.MILLISECONDS),
                "AeronTransport close should wait for in-flight poll");
        } finally {
            pollRaceAdapter.releasePoll.countDown();
            pollRaceAdapter.releaseClose.countDown();
        }
        join(poller, "poll thread should finish");
        join(pollCloser, "close thread should finish");
        ensure(pollRaceAdapter.closeAttempts == 1, "AeronTransport close after concurrent poll mismatch");

        AeronTransport.AeronConfig failingConfig = new AeronTransport.AeronConfig(
            "aeron:ipc", 42, "", 64, 2, new org.agrona.concurrent.BusySpinIdleStrategy(), false, false, false);
        AeronTransport failingTransport = new AeronTransport(
            failingConfig,
            new FakeAeronAdapter(io.aeron.Publication.BACK_PRESSURED, io.aeron.Publication.BACK_PRESSURED));
        try {
            boolean failed = false;
            try {
                failingTransport.send(new Engine.Message(1, 1, 1, 1, 1, 1, 1));
            } catch (IllegalStateException ex) {
                failed = true;
            }
            ensure(failed, "AeronTransport offer failure mismatch");
            ensure(failingTransport.stats().offerBackPressure() == 2, "AeronTransport stats back pressure mismatch");
        } finally {
            failingTransport.close();
        }

        // Offer failure classification: each Aeron publication result code must
        // land in its own stats counter (and only there).
        long[] failureCodes = {
            io.aeron.Publication.NOT_CONNECTED,
            io.aeron.Publication.ADMIN_ACTION,
            io.aeron.Publication.CLOSED,
            -99L,
        };
        for (long code : failureCodes) {
            AeronTransport codeTransport = new AeronTransport(
                failingConfig, new FakeAeronAdapter(code, code));
            try {
                boolean codeFailed = false;
                try {
                    codeTransport.send(new Engine.Message(1, 1, 1, 1, 1, 1, 1));
                } catch (IllegalStateException ex) {
                    codeFailed = true;
                }
                ensure(codeFailed, "AeronTransport offer failure mismatch for code " + code);
                AeronTransport.AeronStats codeStats = codeTransport.stats();
                long expected = 2;
                ensure(codeStats.offerBackPressure() == 0, "AeronTransport stats back pressure misclassification");
                ensure(codeStats.sentCount() == 0, "AeronTransport stats sent on failure mismatch");
                ensure(codeStats.receivedCount() == 0, "AeronTransport stats received on failure mismatch");
                long notConnected = code == io.aeron.Publication.NOT_CONNECTED ? expected : 0;
                long adminAction = code == io.aeron.Publication.ADMIN_ACTION ? expected : 0;
                long closed = code == io.aeron.Publication.CLOSED ? expected : 0;
                long failed = code == -99L ? expected : 0;
                ensure(codeStats.offerNotConnected() == notConnected, "AeronTransport stats not connected mismatch");
                ensure(codeStats.offerAdminAction() == adminAction, "AeronTransport stats admin action mismatch");
                ensure(codeStats.offerClosed() == closed, "AeronTransport stats closed mismatch");
                ensure(codeStats.offerFailed() == failed, "AeronTransport stats failed mismatch");
            } finally {
                codeTransport.close();
            }
        }

        // Source-seq tie-break: identical epoch/channel/qos/sourceId must order by
        // sourceSeq and still aggregate into one epoch result.
        List<Engine.Message> tieMessages = new ArrayList<>();
        tieMessages.add(new Engine.Message(4, 1, 1, 7, 20, 0, 3));
        tieMessages.add(new Engine.Message(4, 1, 1, 7, 10, 0, 4));
        List<Engine.EpochResult> tieResults = Engine.processMessages(tieMessages);
        ensure(tieResults.size() == 1 && tieResults.get(0).state == 7, "Source-seq tie state mismatch");

        // Hash padding: a state hash with a leading zero nibble must still render
        // as exactly 16 lowercase hex characters (zero-padded, not truncated).
        long padPayload = -1;
        for (long p = 1; p <= 20000 && padPayload < 0; p++) {
            if (Engine.fnv1a64Hex("state:" + p).startsWith("0")) {
                padPayload = p;
            }
        }
        ensure(padPayload > 0, "no padding vector found");
        List<Engine.Message> padMessages = new ArrayList<>();
        padMessages.add(new Engine.Message(9, 1, 0, 1, 1, 0, padPayload));
        List<Engine.EpochResult> padResults = Engine.processMessages(padMessages);
        ensure(padResults.size() == 1, "Padding result count mismatch");
        ensure(padResults.get(0).hash.length() == 16, "Padded hash length mismatch");
        ensure(padResults.get(0).hash.startsWith("0"), "Padded hash leading zero mismatch");
        ensure(padResults.get(0).hash.equals(Engine.fnv1a64Hex("state:" + padPayload)), "Padded hash mismatch");

        // Short-frame tolerance: a fragment shorter than the frame length must be
        // ignored by the poll path without corrupting the batch.
        FakeAeronAdapter shortFrameAdapter = new FakeAeronAdapter();
        AeronTransport shortFrameTransport = new AeronTransport(
            new AeronTransport.AeronConfig(
                "aeron:ipc", 48, "", 64, 10,
                new org.agrona.concurrent.BusySpinIdleStrategy(), false, false, false),
            shortFrameAdapter);
        try {
            shortFrameAdapter.seedFrame(new byte[10]);
            ensure(shortFrameTransport.poll(1).isEmpty(), "AeronTransport short frame mismatch");
        } finally {
            shortFrameTransport.close();
        }

        // Real Aeron adapter idempotent re-close: the partial-close retry path in
        // AeronTransport.close() assumes RealAeronAdapter.close() can be called again
        // after a partial release (publication/subscription/aeron/mediaDriver each
        // close idempotently). Drive the real embedded Aeron stack directly and close
        // the adapter twice, asserting no exception on the repeated close.
        AeronTransport.AeronConfig realConfig = new AeronTransport.AeronConfig(
            "aeron:ipc", 46, "", 64, 10, new org.agrona.concurrent.BusySpinIdleStrategy(), true, true, true);
        AeronTransport.RealAeronAdapter realAdapter = new AeronTransport.RealAeronAdapter(realConfig);
        realAdapter.close();
        realAdapter.close();
        realAdapter.close();

        // Transport-level contract still holds: after a successful close the adapter is
        // fully released and a second transport close is a no-op.
        AeronTransport realTransport = new AeronTransport(realConfig);
        realTransport.close();
        ensure(realTransport.poll(1).isEmpty(), "Real Aeron transport poll after close mismatch");
        boolean realSendAfterCloseFailed = false;
        try {
            realTransport.send(new Engine.Message(1, 1, 1, 1, 1, 1, 9));
        } catch (IllegalStateException ex) {
            realSendAfterCloseFailed = true;
        }
        ensure(realSendAfterCloseFailed, "Real Aeron transport send after close mismatch");
        realTransport.close();

        // Real Aeron end-to-end roundtrip over an explicitly configured driver
        // directory: exercises adapter construction with a non-blank directory,
        // real publication offer and subscription poll.
        java.nio.file.Path realIoDir = Files.createTempDirectory("epoch-aeron-io");
        AeronTransport.AeronConfig realIoConfig = new AeronTransport.AeronConfig(
            "aeron:ipc", 49, realIoDir.toString(), 64, 10,
            new org.agrona.concurrent.BusySpinIdleStrategy(), true, true, true);
        AeronTransport realIoTransport = new AeronTransport(realIoConfig);
        try {
            realIoTransport.send(new Engine.Message(2, 1, 1, 1, 1, 1, 11));
            List<Engine.Message> realIoOut = List.of();
            for (int i = 0; i < 200 && realIoOut.isEmpty(); i++) {
                realIoOut = realIoTransport.poll(1);
                if (realIoOut.isEmpty()) {
                    Thread.sleep(5);
                }
            }
            ensure(!realIoOut.isEmpty(), "Real Aeron roundtrip received nothing");
            ensure(realIoOut.get(0).payload == 11, "Real Aeron roundtrip payload mismatch");
            ensure(realIoTransport.config().streamId() == 49, "Real Aeron config accessor mismatch");
            ensure(realIoTransport.stats().sentCount() == 1, "Real Aeron roundtrip stats mismatch");
        } finally {
            realIoTransport.close();
        }

        Path vector = locateVectorFile();
        List<Engine.Message> messages = new ArrayList<>();
        List<Expected> expected = new ArrayList<>();

        for (String line : Files.readAllLines(vector)) {
            if (line.isBlank() || line.startsWith("#")) {
                continue;
            }
            String[] parts = line.split(",");
            if (parts.length == 0) {
                continue;
            }
            if ("M".equals(parts[0]) && (parts.length == 7 || parts.length == 8)) {
                int qos = parts.length == 8 ? Integer.parseInt(parts[6]) : 0;
                long payload = parts.length == 8 ? Long.parseLong(parts[7]) : Long.parseLong(parts[6]);
                messages.add(new Engine.Message(
                    Long.parseLong(parts[1]),
                    Long.parseLong(parts[2]),
                    Long.parseLong(parts[3]),
                    Long.parseLong(parts[4]),
                    Long.parseLong(parts[5]),
                    qos,
                    payload
                ));
            } else if ("E".equals(parts[0]) && parts.length == 4) {
                expected.add(new Expected(
                    Long.parseLong(parts[1]),
                    Long.parseLong(parts[2]),
                    parts[3]
                ));
            }
        }

        List<Engine.EpochResult> results = Engine.processMessages(messages);
        if (results.size() != expected.size()) {
            throw new IllegalStateException("Result count mismatch");
        }
        for (int i = 0; i < expected.size(); i++) {
            Expected exp = expected.get(i);
            Engine.EpochResult res = results.get(i);
            if (exp.epoch != res.epoch || exp.state != res.state || !exp.hash.equals(res.hash)) {
                throw new IllegalStateException("Vector mismatch at index " + i);
            }
        }
    }

    private static void ensure(boolean condition, String message) {
        if (!condition) {
            throw new IllegalStateException(message);
        }
    }

    private static void await(java.util.concurrent.CountDownLatch latch, long ms, String message) throws InterruptedException {
        ensure(latch.await(ms, java.util.concurrent.TimeUnit.MILLISECONDS), message);
    }

    private static void join(Thread thread, String message) throws InterruptedException {
        thread.join(5000);
        ensure(!thread.isAlive(), message);
    }

    private static final class FakeAeronAdapter implements AeronTransport.AeronAdapter {
        private final List<byte[]> frames = new ArrayList<>();
        private final List<Long> offerResults = new ArrayList<>();
        private final CountDownLatch closeStarted = new CountDownLatch(1);
        private final AtomicBoolean blockClose = new AtomicBoolean(false);
        private final CountDownLatch releaseClose = new CountDownLatch(1);
        private final CountDownLatch offerStarted = new CountDownLatch(1);
        private final CountDownLatch releaseOffer = new CountDownLatch(1);
        private final AtomicBoolean blockOffer = new AtomicBoolean(false);
        private final CountDownLatch pollStarted = new CountDownLatch(1);
        private final CountDownLatch releasePoll = new CountDownLatch(1);
        private final AtomicBoolean blockPoll = new AtomicBoolean(false);
        private int closeAttempts;
        private boolean failFirstClose;
        private int failOnResource = -1;
        private int publicationsReleased;
        private int subscriptionsReleased;
        private int aeronsReleased;
        private int mediaDriversReleased;
        private int publicationCloseCalls;
        private int subscriptionCloseCalls;
        private int aeronCloseCalls;
        private int mediaDriverCloseCalls;
        private boolean publicationClosed;
        private boolean subscriptionClosed;
        private boolean aeronClosed;
        private boolean mediaDriverClosed;

        FakeAeronAdapter(long... results) {
            for (long result : results) {
                offerResults.add(result);
            }
        }

        void seedFrame(byte[] frame) {
            frames.add(frame);
        }

        @Override
        public long offer(org.agrona.DirectBuffer buffer, int offset, int length) {
            if (blockOffer.get()) {
                offerStarted.countDown();
                try {
                    releaseOffer.await();
                } catch (InterruptedException e) {
                    Thread.currentThread().interrupt();
                }
            }
            if (!offerResults.isEmpty()) {
                long result = offerResults.remove(0);
                if (result < 0) {
                    return result;
                }
            }
            byte[] copy = new byte[length];
            buffer.getBytes(offset, copy);
            frames.add(copy);
            return 1;
        }

        @Override
        public int poll(io.aeron.logbuffer.FragmentHandler handler, int fragmentLimit) {
            if (blockPoll.get()) {
                pollStarted.countDown();
                try {
                    releasePoll.await();
                } catch (InterruptedException e) {
                    Thread.currentThread().interrupt();
                }
            }
            int count = 0;
            while (count < fragmentLimit && !frames.isEmpty()) {
                byte[] frame = frames.remove(0);
                handler.onFragment(new org.agrona.concurrent.UnsafeBuffer(frame), 0, frame.length, null);
                count++;
            }
            return count;
        }

        @Override
        public void close() {
            if (blockClose.get()) {
                closeStarted.countDown();
                try {
                    releaseClose.await();
                } catch (InterruptedException e) {
                    Thread.currentThread().interrupt();
                }
            }
            closeAttempts++;
            if (failFirstClose && closeAttempts == 1) {
                throw new IllegalStateException("AeronTransport close failure mismatch");
            }
            if (!publicationClosed) {
                publicationCloseCalls++;
                if (failOnResource == 1 && closeAttempts == 1) {
                    throw new IllegalStateException("AeronTransport partial close failure mismatch");
                }
                publicationsReleased++;
                publicationClosed = true;
            }
            if (!subscriptionClosed) {
                subscriptionCloseCalls++;
                if (failOnResource == 2 && closeAttempts == 1) {
                    throw new IllegalStateException("AeronTransport partial close failure mismatch");
                }
                subscriptionsReleased++;
                subscriptionClosed = true;
            }
            if (!aeronClosed) {
                aeronCloseCalls++;
                if (failOnResource == 3 && closeAttempts == 1) {
                    throw new IllegalStateException("AeronTransport partial close failure mismatch");
                }
                aeronsReleased++;
                aeronClosed = true;
            }
            if (!mediaDriverClosed) {
                mediaDriverCloseCalls++;
                if (failOnResource == 4 && closeAttempts == 1) {
                    throw new IllegalStateException("AeronTransport partial close failure mismatch");
                }
                mediaDriversReleased++;
                mediaDriverClosed = true;
            }
            frames.clear();
        }
    }

    private static Path locateVectorFile() {
        Path current = Path.of(".").toAbsolutePath();
        for (int i = 0; i < 6; i++) {
            Path candidate = current.resolve("test-vectors").resolve("epoch_vector_v1.txt");
            if (Files.exists(candidate)) {
                return candidate;
            }
            current = current.getParent();
            if (current == null) {
                break;
            }
        }
        throw new IllegalStateException("Vector file not found");
    }

    private record Expected(long epoch, long state, String hash) {
    }
}
