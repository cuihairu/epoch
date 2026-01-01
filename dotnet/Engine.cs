using System.Globalization;

namespace Epoch;

public readonly struct Message
{
    public Message(long epoch, long channelId, long sourceId, long sourceSeq, long schemaId, long payload)
    {
        Epoch = epoch;
        ChannelId = channelId;
        SourceId = sourceId;
        SourceSeq = sourceSeq;
        SchemaId = schemaId;
        Payload = payload;
    }

    public long Epoch { get; }
    public long ChannelId { get; }
    public long SourceId { get; }
    public long SourceSeq { get; }
    public long SchemaId { get; }
    public long Payload { get; }
}

public readonly struct EpochResult
{
    public EpochResult(long epoch, long state, string hash)
    {
        Epoch = epoch;
        State = state;
        Hash = hash;
    }

    public long Epoch { get; }
    public long State { get; }
    public string Hash { get; }
}

public static class Engine
{
    private const ulong FnvOffsetBasis = 0xcbf29ce484222325UL;
    private const ulong FnvPrime = 0x100000001b3UL;

    public static string Fnv1A64Hex(string input)
    {
        ulong hash = FnvOffsetBasis;
        foreach (var b in System.Text.Encoding.UTF8.GetBytes(input))
        {
            hash ^= b;
            hash *= FnvPrime;
        }

        return hash.ToString("x16", CultureInfo.InvariantCulture);
    }

    public static IReadOnlyList<EpochResult> ProcessMessages(List<Message> messages)
    {
        messages.Sort((a, b) =>
        {
            var cmp = a.Epoch.CompareTo(b.Epoch);
            if (cmp != 0) return cmp;
            cmp = a.ChannelId.CompareTo(b.ChannelId);
            if (cmp != 0) return cmp;
            cmp = a.SourceId.CompareTo(b.SourceId);
            if (cmp != 0) return cmp;
            return a.SourceSeq.CompareTo(b.SourceSeq);
        });

        var results = new List<EpochResult>();
        var state = 0L;
        long currentEpoch = 0;
        var hasEpoch = false;

        foreach (var message in messages)
        {
            if (!hasEpoch)
            {
                currentEpoch = message.Epoch;
                hasEpoch = true;
            }
            if (message.Epoch != currentEpoch)
            {
                results.Add(new EpochResult(currentEpoch, state, Fnv1A64Hex($"state:{state}")));
                currentEpoch = message.Epoch;
            }
            state += message.Payload;
        }

        if (hasEpoch)
        {
            results.Add(new EpochResult(currentEpoch, state, Fnv1A64Hex($"state:{state}")));
        }

        return results;
    }
}
