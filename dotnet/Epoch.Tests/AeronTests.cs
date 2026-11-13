using System.Collections.Generic;
using Epoch;
using Xunit;

namespace Epoch.Tests;

public class AeronTests
{
    [Fact]
    public void AeronTransportRoundTrip()
    {
        var native = new FakeNative();
        var transport = new AeronTransport(new AeronTransport.AeronConfig("aeron:ipc", 1, ""), native);
        transport.Send(new Message(1, 2, 3, 4, 5, 6, -7));
        transport.Send(new Message(2, 3, 4, 5, 6, 1, 9));

        var first = transport.Poll(1);
        Assert.Single(first);
        Assert.Equal(-7, first[0].Payload);

        var second = transport.Poll(5);
        Assert.Single(second);
        Assert.Equal(9, second[0].Payload);

        transport.Close();
        Assert.Empty(transport.Poll(1));
    }

    [Fact]
    public void PollSkipsShortAndUnsupportedFrames()
    {
        var native = new FakeNative();
        var transport = new AeronTransport(new AeronTransport.AeronConfig("aeron:ipc", 2, ""), native);
        native.Enqueue(new byte[4]);

        var frame = new byte[56];
        frame[0] = 2;
        native.Enqueue(frame);

        Assert.Empty(transport.Poll(2));
        Assert.Equal(2, transport.Stats.ReceivedErrors);
        transport.Close();
        Assert.Equal(2, transport.Stats.ReceivedErrors);
    }

    [Fact]
    public void PollSkipsPoisonFramesAndDeliversGoodOnes()
    {
        var native = new FakeNative();
        var transport = new AeronTransport(new AeronTransport.AeronConfig("aeron:ipc", 7, ""), native);
        var good = new byte[56];
        good[0] = 1;
        System.Buffers.Binary.BinaryPrimitives.WriteInt64LittleEndian(good.AsSpan(48, 8), 99);
        var bad = (byte[])good.Clone();
        bad[0] = 2;
        native.Enqueue(bad);
        native.Enqueue(bad);
        native.Enqueue(good);
        native.ReturnedStats = new AeronTransport.AeronStats(0, 1, 0, 0, 0, 0, 0, 0);

        var batch = transport.Poll(4);
        Assert.Single(batch);
        Assert.Equal(99, batch[0].Payload);
        Assert.Equal(2, transport.Stats.ReceivedErrors);
        Assert.Equal(1, transport.Stats.ReceivedCount);
    }

    [Fact]
    public void TransportExposesConfigAndClosedLifecycle()
    {
        var config = new AeronTransport.AeronConfig("aeron:ipc", 3, "", 0, 0);
        var native = new FakeNative();
        var transport = new AeronTransport(config, native);
        Assert.Equal(config, transport.Config);
        var message = new Message(1, 1, 1, 1, 1, 1, 1);
        native.SendResults.Add(-1);
        Assert.Throws<InvalidOperationException>(() => transport.Send(message));
        native.Enqueue(new byte[4]);
        Assert.Empty(transport.Poll(1));
        Assert.Equal(1, transport.Stats.ReceivedErrors);

        transport.Close();
        transport.Close();
        Assert.Throws<InvalidOperationException>(() => transport.Send(new Message(1, 1, 1, 1, 1, 1, 1)));
        Assert.Empty(transport.Poll(1));
        Assert.Equal(0, transport.Stats.SentCount);
    }

    [Fact]
    public void TransportReportsStatsAndFailedOffer()
    {
        var native = new FakeNative
        {
            ReturnedStats = new AeronTransport.AeronStats(1, 2, 3, 4, 5, 6, 7, 8),
            SendResults = { -1 }
        };
        var transport = new AeronTransport(new AeronTransport.AeronConfig("aeron:ipc", 4, ""), native);
        Assert.Equal(1, transport.Stats.SentCount);
        Assert.Equal(8, transport.Stats.OfferFailed);
        Assert.Throws<InvalidOperationException>(() => transport.Send(new Message(1, 1, 1, 1, 1, 1, 1)));
        transport.Close();
    }

    [Fact]
    public void StatsExposeAllFailureCounters()
    {
        var native = new FakeNative
        {
            ReturnedStats = new AeronTransport.AeronStats(1, 2, 3, 4, 5, 6, 7, 8)
        };
        var transport = new AeronTransport(new AeronTransport.AeronConfig("aeron:ipc", 5, ""), native);
        var stats = transport.Stats;
        Assert.Equal(1, stats.SentCount);
        Assert.Equal(2, stats.ReceivedCount);
        Assert.Equal(3, stats.OfferBackPressure);
        Assert.Equal(4, stats.OfferNotConnected);
        Assert.Equal(5, stats.OfferAdminAction);
        Assert.Equal(6, stats.OfferClosed);
        Assert.Equal(7, stats.OfferMaxPosition);
        Assert.Equal(8, stats.OfferFailed);
        transport.Close();
    }

    private sealed class FakeNative : AeronTransport.IAeronNative
    {
        private readonly Queue<byte[]> frames = new();

        public AeronTransport.AeronStats ReturnedStats { get; set; } = new(0, 0, 0, 0, 0, 0, 0, 0);

        public List<int> SendResults { get; } = new();

        public IntPtr Open(AeronTransport.AeronConfig config) => new IntPtr(1);

        public void Send(IntPtr transport, byte[] frame)
        {
            if (SendResults.Count > 0 && SendResults[0] < 0)
            {
                throw new InvalidOperationException("native send failed");
            }
            if (SendResults.Count > 0)
            {
                SendResults.RemoveAt(0);
            }
            frames.Enqueue(frame);
        }

        public void Enqueue(byte[] frame) => frames.Enqueue(frame);

        public List<byte[]> Poll(IntPtr transport, int max)
        {
            var output = new List<byte[]>();
            while (output.Count < max && frames.Count > 0)
            {
                output.Add(frames.Dequeue());
            }
            return output;
        }

        public AeronTransport.AeronStats Stats(IntPtr transport) => ReturnedStats;

        public void Close(IntPtr transport) { }
    }
}
