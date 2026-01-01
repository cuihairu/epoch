#include "epoch/engine.h"

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
