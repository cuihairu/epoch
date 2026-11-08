using System;
using System.Collections.Generic;
using Epoch;
using Xunit;

namespace Epoch.Tests;

public class CoverageTests
{
    [Fact]
    public void AeronConfigExplicitValuesRoundTrip()
    {
        var config = new AeronTransport.AeronConfig(
            "aeron:udp?endpoint=127.0.0.1:20001", 7, "", 32, 5);
        Assert.Equal(32, config.FragmentLimit);
        Assert.Equal(5, config.OfferMaxAttempts);
    }

    [Fact]
    public void DefaultCodecNameIsDefault()
    {
        Assert.Equal("default", new DefaultActorIdCodec().Name);
    }

    [Fact]
    public void ProcessMessagesOrdersBySourceSeqWithinKey()
    {
        var messages = new List<Message>
        {
            new Message(4, 1, 2, 7, 20, 0, 3),
            new Message(4, 1, 2, 7, 10, 0, 4),
        };
        var results = Engine.ProcessMessages(messages);
        Assert.Single(results);
        Assert.Equal(7, results[0].State);
    }

    [Fact]
    public void RealNativeOpenRejectsInvalidDirectory()
    {
        try
        {
            var ex = Assert.Throws<InvalidOperationException>(() =>
                new AeronTransport(new AeronTransport.AeronConfig(
                    "aeron:ipc", 9, "/dev/null/epoch-denied", 64, 10)));
            Assert.False(string.IsNullOrEmpty(ex.Message));
        }
        catch (DllNotFoundException)
        {
            // Native library not available in this environment; skip.
        }
    }
}
