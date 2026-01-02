#include "epoch/engine.h"
#include "epoch/epoch.h"
#include "epoch/transport.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {

struct Expected {
    std::int64_t epoch;
    std::int64_t state;
    std::string hash;
};

std::filesystem::path locate_vector_file()
{
    auto current = std::filesystem::current_path();
    for (int i = 0; i < 6; ++i)
    {
        auto candidate = current / "test-vectors" / "epoch_vector_v1.txt";
        if (std::filesystem::exists(candidate))
        {
            return candidate;
        }
        if (!current.has_parent_path())
        {
            break;
        }
        current = current.parent_path();
    }
    return {};
}

void parse_vector_file(const std::filesystem::path &path,
                       std::vector<epoch::Message> &messages,
                       std::vector<Expected> &expected)
{
    std::ifstream in(path);
    std::string line;
    while (std::getline(in, line))
    {
        if (line.empty() || line[0] == '#')
        {
            continue;
        }
        std::stringstream ss(line);
        std::string token;
        std::vector<std::string> parts;
        while (std::getline(ss, token, ','))
        {
            parts.push_back(token);
        }
        if (parts.empty())
        {
            continue;
        }
        if (parts[0] == "M" && parts.size() == 7)
        {
            messages.push_back({
                std::stoll(parts[1]),
                std::stoll(parts[2]),
                std::stoll(parts[3]),
                std::stoll(parts[4]),
                std::stoll(parts[5]),
                std::stoll(parts[6])
            });
        }
        else if (parts[0] == "E" && parts.size() == 4)
        {
            expected.push_back({
                std::stoll(parts[1]),
                std::stoll(parts[2]),
                parts[3]
            });
        }
    }
}

} // namespace

int main()
{
    if (std::string(epoch::version()) != "0.1.0")
    {
        return 1;
    }
    if (epoch::fnv1a64_hex("state:0") != "c3c43df01be7b59c")
    {
        return 1;
    }
    if (epoch::fnv1a64_hex("hello") != "a430d84680aabd0b")
    {
        return 1;
    }
    if (!epoch::process_messages({}).empty())
    {
        return 1;
    }

    std::vector<epoch::Message> sample_messages = {
        {2, 2, 1, 2, 100, 5},
        {1, 1, 2, 1, 100, 2},
        {1, 1, 1, 2, 100, -1},
        {3, 1, 1, 1, 100, 4},
    };
    auto sample_results = epoch::process_messages(sample_messages);
    if (sample_results.size() != 3)
    {
        return 1;
    }
    if (sample_results[0].epoch != 1 || sample_results[0].state != 1 ||
        sample_results[0].hash != "c3c43ef01be7b74f")
    {
        return 1;
    }
    if (sample_results[1].epoch != 2 || sample_results[1].state != 6 ||
        sample_results[1].hash != "c3c43bf01be7b236")
    {
        return 1;
    }
    if (sample_results[2].epoch != 3 || sample_results[2].state != 10 ||
        sample_results[2].hash != "8e2e70ff6abccccd")
    {
        return 1;
    }

    epoch::InMemoryTransport transport;
    transport.send({1, 1, 1, 1, 100, 5});
    transport.send({1, 1, 1, 2, 100, 7});
    transport.send({2, 1, 1, 3, 100, 9});
    if (!transport.poll(0).empty())
    {
        return 1;
    }
    auto first_batch = transport.poll(2);
    if (first_batch.size() != 2)
    {
        return 1;
    }
    if (first_batch[0].payload != 5 || first_batch[1].payload != 7)
    {
        return 1;
    }
    auto second_batch = transport.poll(5);
    if (second_batch.size() != 1 || second_batch[0].payload != 9)
    {
        return 1;
    }
    transport.close();
    if (!transport.poll(1).empty())
    {
        return 1;
    }

    std::vector<epoch::Message> messages;
    std::vector<Expected> expected;

    auto path = locate_vector_file();
    if (path.empty())
    {
        return 1;
    }
    parse_vector_file(path, messages, expected);

    auto results = epoch::process_messages(messages);
    if (results.size() != expected.size())
    {
        return 1;
    }

    for (std::size_t i = 0; i < expected.size(); ++i)
    {
        if (results[i].epoch != expected[i].epoch)
        {
            return 1;
        }
        if (results[i].state != expected[i].state)
        {
            return 1;
        }
        if (results[i].hash != expected[i].hash)
        {
            return 1;
        }
    }

    return 0;
}
