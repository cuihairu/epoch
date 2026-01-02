namespace Epoch;

public readonly struct ActorIdParts
{
    public ActorIdParts(int region, int server, int processType, int processIndex, int actorIndex)
    {
        Region = region;
        Server = server;
        ProcessType = processType;
        ProcessIndex = processIndex;
        ActorIndex = actorIndex;
    }

    public int Region { get; }
    public int Server { get; }
    public int ProcessType { get; }
    public int ProcessIndex { get; }
    public int ActorIndex { get; }
}

public interface IActorIdCodec
{
    long Encode(ActorIdParts parts);
    ActorIdParts Decode(long value);
    string Name { get; }
}

public sealed class DefaultActorIdCodec : IActorIdCodec
{
    private const int RegionBits = 10;
    private const int ServerBits = 12;
    private const int ProcessTypeBits = 6;
    private const int ProcessIndexBits = 10;
    private const int ActorIndexBits = 26;

    private const ulong RegionMax = (1UL << RegionBits) - 1;
    private const ulong ServerMax = (1UL << ServerBits) - 1;
    private const ulong ProcessTypeMax = (1UL << ProcessTypeBits) - 1;
    private const ulong ProcessIndexMax = (1UL << ProcessIndexBits) - 1;
    private const ulong ActorIndexMax = (1UL << ActorIndexBits) - 1;

    private const int ActorIndexShift = 0;
    private const int ProcessIndexShift = ActorIndexShift + ActorIndexBits;
    private const int ProcessTypeShift = ProcessIndexShift + ProcessIndexBits;
    private const int ServerShift = ProcessTypeShift + ProcessTypeBits;
    private const int RegionShift = ServerShift + ServerBits;

    public static readonly DefaultActorIdCodec Instance = new();

    public long Encode(ActorIdParts parts)
    {
        if (parts.Region < 0 || (ulong)parts.Region > RegionMax ||
            parts.Server < 0 || (ulong)parts.Server > ServerMax ||
            parts.ProcessType < 0 || (ulong)parts.ProcessType > ProcessTypeMax ||
            parts.ProcessIndex < 0 || (ulong)parts.ProcessIndex > ProcessIndexMax ||
            parts.ActorIndex < 0 || (ulong)parts.ActorIndex > ActorIndexMax)
        {
            throw new ArgumentOutOfRangeException(nameof(parts), "ActorId parts out of range");
        }

        ulong value = ((ulong)parts.Region << RegionShift)
            | ((ulong)parts.Server << ServerShift)
            | ((ulong)parts.ProcessType << ProcessTypeShift)
            | ((ulong)parts.ProcessIndex << ProcessIndexShift)
            | ((ulong)parts.ActorIndex << ActorIndexShift);

        return unchecked((long)value);
    }

    public ActorIdParts Decode(long value)
    {
        ulong raw = unchecked((ulong)value);
        int region = (int)((raw >> RegionShift) & RegionMax);
        int server = (int)((raw >> ServerShift) & ServerMax);
        int processType = (int)((raw >> ProcessTypeShift) & ProcessTypeMax);
        int processIndex = (int)((raw >> ProcessIndexShift) & ProcessIndexMax);
        int actorIndex = (int)((raw >> ActorIndexShift) & ActorIndexMax);
        return new ActorIdParts(region, server, processType, processIndex, actorIndex);
    }

    public string Name => "default";
}

public static class ActorId
{
    public static IActorIdCodec Default => DefaultActorIdCodec.Instance;

    public static long Encode(ActorIdParts parts) => Default.Encode(parts);

    public static ActorIdParts Decode(long value) => Default.Decode(value);
}
