using System.Globalization;
using Epoch;

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
if (results.Count != expected.Count)
{
    Environment.Exit(1);
}

for (var i = 0; i < expected.Count; i++)
{
    var exp = expected[i];
    var res = results[i];
    if (exp.Epoch != res.Epoch || exp.State != res.State || exp.Hash != res.Hash)
    {
        Environment.Exit(1);
    }
}

static string LocateVectorFile()
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

record Expected(long Epoch, long State, string Hash);
