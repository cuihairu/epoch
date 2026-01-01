namespace Epoch;

public interface ITransport
{
    void Send(Message message);
    List<Message> Poll(int max);
    void Close();
}

public sealed class InMemoryTransport : ITransport
{
    private readonly Queue<Message> _queue = new();

    public void Send(Message message)
    {
        _queue.Enqueue(message);
    }

    public List<Message> Poll(int max)
    {
        var results = new List<Message>();
        while (_queue.Count > 0 && results.Count < max)
        {
            results.Add(_queue.Dequeue());
        }
        return results;
    }

    public void Close()
    {
        _queue.Clear();
    }
}
