#include "epoch/aeron_transport.h"
#include "epoch/engine.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <deque>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr std::uint8_t kFrameVersion = 1;
constexpr std::size_t kFrameLength = 56;
constexpr std::size_t kOffsetVersion = 0;
constexpr std::size_t kOffsetQos = 1;
constexpr std::size_t kOffsetEpoch = 8;
constexpr std::size_t kOffsetChannel = 16;
constexpr std::size_t kOffsetSource = 24;
constexpr std::size_t kOffsetSourceSeq = 32;
constexpr std::size_t kOffsetSchema = 40;
constexpr std::size_t kOffsetPayload = 48;

struct Frame {
    std::vector<std::uint8_t> data;
    std::size_t length;
};

struct StubState {
    std::deque<Frame> frames;
    std::deque<std::int64_t> offer_results;
    bool close_publication = false;
    bool close_subscription = false;
    bool close_client = false;
    bool close_context = false;
    int async_pub_poll_calls = 0;
    int async_sub_poll_calls = 0;
    int context_set_dir_calls = 0;
};

StubState *g_state = nullptr;

void encode_message(Frame &frame, const epoch::Message &message, std::uint8_t version = kFrameVersion)
{
    frame.data.assign(kFrameLength, 0);
    frame.length = kFrameLength;
    frame.data[kOffsetVersion] = version;
    frame.data[kOffsetQos] = message.qos;
    std::memcpy(frame.data.data() + kOffsetEpoch, &message.epoch, sizeof(message.epoch));
    std::memcpy(frame.data.data() + kOffsetChannel, &message.channel_id, sizeof(message.channel_id));
    std::memcpy(frame.data.data() + kOffsetSource, &message.source_id, sizeof(message.source_id));
    std::memcpy(frame.data.data() + kOffsetSourceSeq, &message.source_seq, sizeof(message.source_seq));
    std::memcpy(frame.data.data() + kOffsetSchema, &message.schema_id, sizeof(message.schema_id));
    std::memcpy(frame.data.data() + kOffsetPayload, &message.payload, sizeof(message.payload));
}

int stub_context_init(aeron_context_t **context)
{
    static int dummy = 0;
    *context = reinterpret_cast<aeron_context_t *>(&dummy);
    return 0;
}

int stub_context_set_dir(aeron_context_t *, const char *)
{
    if (g_state != nullptr)
    {
        g_state->context_set_dir_calls++;
    }
    return 0;
}

int stub_init(aeron_t **client, aeron_context_t *)
{
    static int dummy = 0;
    *client = reinterpret_cast<aeron_t *>(&dummy);
    return 0;
}

int stub_start(aeron_t *)
{
    return 0;
}

int stub_async_add_publication(aeron_async_add_publication_t **async, aeron_t *, const char *, int32_t)
{
    static int dummy = 0;
    *async = reinterpret_cast<aeron_async_add_publication_t *>(&dummy);
    return 0;
}

int stub_async_add_publication_poll(aeron_publication_t **publication, aeron_async_add_publication_t *)
{
    static int dummy = 0;
    if (g_state != nullptr)
    {
        g_state->async_pub_poll_calls++;
    }
    *publication = reinterpret_cast<aeron_publication_t *>(&dummy);
    return 1;
}

int stub_async_add_subscription(aeron_async_add_subscription_t **async,
                                aeron_t *,
                                const char *,
                                int32_t,
                                aeron_on_available_image_t,
                                void *,
                                aeron_on_unavailable_image_t,
                                void *)
{
    static int dummy = 0;
    *async = reinterpret_cast<aeron_async_add_subscription_t *>(&dummy);
    return 0;
}

int stub_async_add_subscription_poll(aeron_subscription_t **subscription, aeron_async_add_subscription_t *)
{
    static int dummy = 0;
    if (g_state != nullptr)
    {
        g_state->async_sub_poll_calls++;
    }
    *subscription = reinterpret_cast<aeron_subscription_t *>(&dummy);
    return 1;
}

std::int64_t stub_publication_offer(
    aeron_publication_t *,
    const std::uint8_t *buffer,
    std::size_t length,
    aeron_reserved_value_supplier_t,
    void *)
{
    if (g_state == nullptr)
    {
        return 1;
    }
    if (!g_state->offer_results.empty())
    {
        auto result = g_state->offer_results.front();
        g_state->offer_results.pop_front();
        if (result >= 0)
        {
            Frame frame;
            frame.data.assign(buffer, buffer + length);
            frame.length = length;
            g_state->frames.push_back(frame);
        }
        return result;
    }
    Frame frame;
    frame.data.assign(buffer, buffer + length);
    frame.length = length;
    g_state->frames.push_back(frame);
    return 1;
}

int stub_subscription_poll(
    aeron_subscription_t *,
    aeron_fragment_handler_t handler,
    void *clientd,
    std::size_t fragment_limit)
{
    if (g_state == nullptr)
    {
        return 0;
    }
    std::size_t fragments = 0;
    while (!g_state->frames.empty() && fragments < fragment_limit)
    {
        auto frame = g_state->frames.front();
        g_state->frames.pop_front();
        handler(clientd, frame.data.data(), frame.length, nullptr);
        fragments++;
    }
    return static_cast<int>(fragments);
}

int stub_publication_close(aeron_publication_t *, aeron_notification_t, void *)
{
    if (g_state != nullptr)
    {
        g_state->close_publication = true;
    }
    return 0;
}

int stub_subscription_close(aeron_subscription_t *, aeron_notification_t, void *)
{
    if (g_state != nullptr)
    {
        g_state->close_subscription = true;
    }
    return 0;
}

int stub_close(aeron_t *)
{
    if (g_state != nullptr)
    {
        g_state->close_client = true;
    }
    return 0;
}

int stub_context_close(aeron_context_t *)
{
    if (g_state != nullptr)
    {
        g_state->close_context = true;
    }
    return 0;
}

const char *stub_errmsg()
{
    return "stub error";
}

epoch::detail::AeronHooks build_stub_hooks()
{
    return epoch::detail::AeronHooks{
        stub_context_init,
        stub_context_set_dir,
        stub_init,
        stub_start,
        stub_async_add_publication,
        stub_async_add_publication_poll,
        stub_async_add_subscription,
        stub_async_add_subscription_poll,
        stub_publication_offer,
        stub_subscription_poll,
        stub_publication_close,
        stub_subscription_close,
        stub_close,
        stub_context_close,
        stub_errmsg,
    };
}

bool test_aeron_round_trip()
{
    StubState state;
    g_state = &state;
    auto previous = epoch::test::aeron_hooks();
    epoch::test::aeron_hooks() = build_stub_hooks();

    bool ok = true;
    {
        epoch::AeronConfig config{"aeron:ipc", 10, "", 4, 3};
        epoch::AeronTransport transport(config);

        epoch::Message first{1, 2, 3, 4, 5, 1, 10};
        epoch::Message second{1, 2, 3, 5, 5, 1, 20};
        transport.send(first);
        transport.send(second);

        auto out = transport.poll(4);
        if (out.size() != 2 || out[0].payload != 10 || out[1].payload != 20)
        {
            ok = false;
        }
        if (transport.stats().sent_count != 2 || transport.stats().received_count != 2)
        {
            ok = false;
        }
        transport.close();
        if (!state.close_publication || !state.close_subscription || !state.close_client || !state.close_context)
        {
            ok = false;
        }
        transport.close();
    }

    epoch::test::aeron_hooks() = previous;
    return ok;
}

bool test_aeron_invalid_frame_ignored()
{
    StubState state;
    g_state = &state;
    auto previous = epoch::test::aeron_hooks();
    epoch::test::aeron_hooks() = build_stub_hooks();

    bool ok = true;
    {
        epoch::AeronTransport transport(epoch::AeronConfig{"aeron:ipc", 20, "", 4, 3});
        Frame bad;
        encode_message(bad, epoch::Message{1, 1, 1, 1, 1, 1, 1}, 2);
        state.frames.push_back(bad);
        Frame good;
        encode_message(good, epoch::Message{2, 2, 2, 2, 2, 1, 99});
        state.frames.push_back(good);

        auto out = transport.poll(4);
        if (out.size() != 1 || out[0].payload != 99)
        {
            ok = false;
        }
    }

    epoch::test::aeron_hooks() = previous;
    return ok;
}

bool test_aeron_offer_failures()
{
    StubState state;
    g_state = &state;
    auto previous = epoch::test::aeron_hooks();
    epoch::test::aeron_hooks() = build_stub_hooks();

    bool ok = true;
    {
        epoch::AeronConfig config{"aeron:ipc", 30, "", 4, 2};
        epoch::AeronTransport transport(config);
        state.offer_results.push_back(AERON_PUBLICATION_BACK_PRESSURED);
        state.offer_results.push_back(AERON_PUBLICATION_BACK_PRESSURED);

        try
        {
            transport.send(epoch::Message{1, 1, 1, 1, 1, 1, 1});
            ok = false;
        }
        catch (const std::runtime_error &)
        {
        }

        if (transport.stats().offer_back_pressure != 2)
        {
            ok = false;
        }
    }

    epoch::test::aeron_hooks() = previous;
    return ok;
}

bool test_aeron_constructor_errors()
{
    auto previous = epoch::test::aeron_hooks();
    auto hooks = build_stub_hooks();
    hooks.context_init = [](aeron_context_t **) { return -1; };
    epoch::test::aeron_hooks() = hooks;

    try
    {
        epoch::AeronTransport transport(epoch::AeronConfig{"aeron:ipc", 40, "", 4, 2});
        (void)transport;
        epoch::test::aeron_hooks() = previous;
        return false;
    }
    catch (const std::runtime_error &)
    {
    }

    epoch::test::aeron_hooks() = previous;
    return true;
}

} // namespace

int main()
{
    if (!test_aeron_round_trip())
    {
        return 1;
    }
    if (!test_aeron_invalid_frame_ignored())
    {
        return 1;
    }
    if (!test_aeron_offer_failures())
    {
        return 1;
    }
    if (!test_aeron_constructor_errors())
    {
        return 1;
    }
    return 0;
}
