namespace Epoch;

public sealed class AeronTransport : ITransport
{
    public sealed record AeronConfig(string Channel, int StreamId, string AeronDirectory);

    public AeronTransport(AeronConfig config)
    {
        Config = config;
    }

    public AeronConfig Config { get; }

    public void Send(Message message)
    {
        throw new NotSupportedException("Aeron transport not linked");
    }

    public List<Message> Poll(int max)
    {
        throw new NotSupportedException("Aeron transport not linked");
    }

    public void Close()
    {
    }
}
