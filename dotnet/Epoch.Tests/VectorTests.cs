using System.Globalization;
using Epoch;
using Xunit;

namespace Epoch.Tests;

public class VectorTests
{
    [Fact]
    public void VectorMatchesExpected()
    {
        var vectorPath = LocateVectorFile();
        var messages = new List<Message>();
        var expected = new List<Expected>();

        foreach (var line in File.ReadAllLines(vectorPath))
        {
            if (string.IsNullOrWhiteSpace(line) || line.StartsWith("#", StringComparison.Ordinal))
            {
                continue;
            }
            var parts = line.Split(',');
            if (parts.Length == 0)
            {
                continue;
            }
            if (parts[0] == "M" && parts.Length == 7)
            {
                messages.Add(new Message(
                    long.Parse(parts[1], CultureInfo.InvariantCulture),
                    long.Parse(parts[2], CultureInfo.InvariantCulture),
                    long.Parse(parts[3], CultureInfo.InvariantCulture),
                    long.Parse(parts[4], CultureInfo.InvariantCulture),
                    long.Parse(parts[5], CultureInfo.InvariantCulture),
                    long.Parse(parts[6], CultureInfo.InvariantCulture)
                ));
            }
            else if (parts[0] == "E" && parts.Length == 4)
            {
                expected.Add(new Expected(
                    long.Parse(parts[1], CultureInfo.InvariantCulture),
                    long.Parse(parts[2], CultureInfo.InvariantCulture),
                    parts[3]
                ));
            }
        }

        var results = Engine.ProcessMessages(messages);
        Assert.Equal(expected.Count, results.Count);
        for (var i = 0; i < expected.Count; i++)
        {
            var exp = expected[i];
            var res = results[i];
            Assert.Equal(exp.Epoch, res.Epoch);
            Assert.Equal(exp.State, res.State);
            Assert.Equal(exp.Hash, res.Hash);
        }
    }

    [Fact]
    public void VersionMatchesExpected()
    {
        Assert.Equal("0.1.0", Version.Value);
    }

    [Fact]
    public void Fnv1A64HexMatchesKnownValues()
    {
        Assert.Equal("c3c43df01be7b59c", Engine.Fnv1A64Hex("state:0"));
        Assert.Equal("a430d84680aabd0b", Engine.Fnv1A64Hex("hello"));
    }

    [Fact]
    public void ProcessMessagesEmpty()
    {
        var results = Engine.ProcessMessages(new List<Message>());
        Assert.Empty(results);
    }

    [Fact]
    public void ProcessMessagesOrdering()
    {
        var messages = new List<Message>
        {
            new(2, 2, 1, 2, 100, 5),
            new(1, 1, 2, 1, 100, 2),
            new(1, 1, 1, 2, 100, -1),
            new(3, 1, 1, 1, 100, 4)
        };

        var results = Engine.ProcessMessages(messages);
        Assert.Equal(3, results.Count);
        Assert.Equal(1, results[0].Epoch);
        Assert.Equal(1, results[0].State);
        Assert.Equal("c3c43ef01be7b74f", results[0].Hash);
        Assert.Equal(2, results[1].Epoch);
        Assert.Equal(6, results[1].State);
        Assert.Equal("c3c43bf01be7b236", results[1].Hash);
        Assert.Equal(3, results[2].Epoch);
        Assert.Equal(10, results[2].State);
        Assert.Equal("8e2e70ff6abccccd", results[2].Hash);
    }

    [Fact]
    public void InMemoryTransportPollsAndCloses()
    {
        var transport = new InMemoryTransport();
        transport.Send(new Message(1, 1, 1, 1, 100, 1));
        transport.Send(new Message(1, 1, 1, 2, 100, 2));
        transport.Send(new Message(2, 1, 1, 3, 100, 3));

        Assert.Empty(transport.Poll(0));
        var first = transport.Poll(2);
        Assert.Equal(2, first.Count);
        Assert.Equal(1, first[0].Payload);
        Assert.Equal(2, first[1].Payload);
        var second = transport.Poll(5);
        Assert.Single(second);
        Assert.Equal(3, second[0].Payload);

        transport.Close();
        Assert.Empty(transport.Poll(1));
    }

    private static string LocateVectorFile()
    {
        var current = Directory.GetCurrentDirectory();
        for (var i = 0; i < 6; i++)
        {
            var candidate = Path.Combine(current, "test-vectors", "epoch_vector_v1.txt");
            if (File.Exists(candidate))
            {
                return candidate;
            }
            var parent = Directory.GetParent(current);
            if (parent == null)
            {
                break;
            }
            current = parent.FullName;
        }
        throw new InvalidOperationException("Vector file not found");
    }

    private record Expected(long Epoch, long State, string Hash);
}
