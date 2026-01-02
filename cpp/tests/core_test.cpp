#include "epoch/actor_id.h"
#include "epoch/engine.h"
#include "epoch/epoch.h"
#include "epoch/transport.h"

#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

bool expect_throw(const std::function<void()> &fn)
{
    try
    {
        fn();
        return false;
    }
    catch (const std::exception &)
    {
        return true;
    }
}

bool test_actor_id_errors()
{
    epoch::ActorIdParts parts{1, 1, 1, 1, 1};
    if (epoch::default_actor_id_codec().name() != std::string("default"))
    {
        return false;
    }
    parts.region = 1 << 10;
    if (!expect_throw([&]() { epoch::encode_actor_id(parts); }))
    {
        return false;
    }
    parts.region = 1;
    parts.server = 1 << 12;
    if (!expect_throw([&]() { epoch::encode_actor_id(parts); }))
    {
        return false;
    }
    parts.server = 1;
    parts.process_type = 1 << 6;
    if (!expect_throw([&]() { epoch::encode_actor_id(parts); }))
    {
        return false;
    }
    parts.process_type = 1;
    parts.process_index = 1 << 10;
    if (!expect_throw([&]() { epoch::encode_actor_id(parts); }))
    {
        return false;
    }
    parts.process_index = 1;
    parts.actor_index = 1U << 26;
    if (!expect_throw([&]() { epoch::encode_actor_id(parts); }))
    {
        return false;
    }
    return true;
}

bool test_engine_ordering_branches()
{
    std::vector<epoch::Message> messages = {
        {2, 1, 1, 1, 100, 0, 5},
        {1, 2, 1, 1, 100, 0, 5},
        {1, 1, 2, 1, 100, 0, 5},
        {1, 1, 1, 2, 100, 0, 5},
        {1, 1, 1, 1, 100, 1, 5},
    };
    auto results = epoch::process_messages(messages);
    if (results.empty())
    {
        return false;
    }
    if (epoch::fnv1a64_hex("state:0") != "c3c43df01be7b59c")
    {
        return false;
    }
    if (!epoch::process_messages({}).empty())
    {
        return false;
    }
    return true;
}

bool test_in_memory_transport()
{
    epoch::InMemoryTransport transport;
    transport.send({1, 1, 1, 1, 1, 0, 10});
    transport.send({1, 1, 1, 2, 1, 0, 20});
    if (!transport.poll(0).empty())
    {
        return false;
    }
    auto batch = transport.poll(1);
    if (batch.size() != 1 || batch[0].payload != 10)
    {
        return false;
    }
    auto remaining = transport.poll(2);
    if (remaining.size() != 1 || remaining[0].payload != 20)
    {
        return false;
    }
    transport.close();
    if (!transport.poll(1).empty())
    {
        return false;
    }
    return true;
}

} // namespace

int main()
{
    if (std::string(epoch::version()) != "0.1.0")
    {
        return 1;
    }
    if (!test_actor_id_errors())
    {
        return 1;
    }
    if (!test_engine_ordering_branches())
    {
        return 1;
    }
    if (!test_in_memory_transport())
    {
        return 1;
    }
    return 0;
}
