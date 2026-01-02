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

    private sealed class FakeNative : AeronTransport.IAeronNative
    {
        private readonly Queue<byte[]> frames = new();

        public IntPtr Open(AeronTransport.AeronConfig config) => new IntPtr(1);

        public void Send(IntPtr transport, byte[] frame) => frames.Enqueue(frame);

        public List<byte[]> Poll(IntPtr transport, int max)
        {
            var output = new List<byte[]>();
            while (output.Count < max && frames.Count > 0)
            {
                output.Add(frames.Dequeue());
            }
            return output;
        }

        public AeronTransport.AeronStats Stats(IntPtr transport) =>
            new(0, 0, 0, 0, 0, 0, 0, 0);

        public void Close(IntPtr transport) { }
    }
}
