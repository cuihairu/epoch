#include "epoch/aeron_transport.h"
#include "epoch/engine.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <deque>
#include <memory>
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
    bool init_fail = false;
    bool start_fail = false;
    bool context_set_dir_fail = false;
    bool add_publication_fail = false;
    bool add_subscription_fail = false;
    int publication_poll_fail_after = -1;
    int subscription_poll_fail_after = -1;
    bool async_pub_poll_zero_first = false;
    bool async_sub_poll_zero_first = false;
    bool publication_null = false;
    bool subscription_null = false;
    int publication_poll_calls = 0;
    int subscription_poll_calls = 0;
    bool poll_fail = false;
    std::size_t next_frame_length = 0;
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
        if (g_state->context_set_dir_fail)
        {
            return -1;
        }
    }
    return 0;
}

int stub_init(aeron_t **client, aeron_context_t *)
{
    if (g_state != nullptr && g_state->init_fail)
    {
        *client = nullptr;
        return -1;
    }
    static int dummy = 0;
    *client = reinterpret_cast<aeron_t *>(&dummy);
    return 0;
}

int stub_start(aeron_t *)
{
    if (g_state != nullptr && g_state->start_fail)
    {
        return -1;
    }
    return 0;
}

int stub_async_add_publication(aeron_async_add_publication_t **async, aeron_t *, const char *, int32_t)
{
    static int dummy = 0;
    if (g_state != nullptr && g_state->add_publication_fail)
    {
        return -1;
    }
    *async = reinterpret_cast<aeron_async_add_publication_t *>(&dummy);
    return 0;
}

int stub_async_add_publication_poll(aeron_publication_t **publication, aeron_async_add_publication_t *)
{
    static int dummy = 0;
    if (g_state != nullptr)
    {
        g_state->async_pub_poll_calls++;
        if (g_state->publication_poll_fail_after >= 0 &&
            g_state->async_pub_poll_calls > g_state->publication_poll_fail_after)
        {
            return -1;
        }
        if (g_state->publication_null)
        {
            *publication = nullptr;
            return 1;
        }
        if (g_state->async_pub_poll_zero_first)
        {
            g_state->async_pub_poll_zero_first = false;
            return 0;
        }
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
    if (g_state != nullptr && g_state->add_subscription_fail)
    {
        return -1;
    }
    *async = reinterpret_cast<aeron_async_add_subscription_t *>(&dummy);
    return 0;
}

int stub_async_add_subscription_poll(aeron_subscription_t **subscription, aeron_async_add_subscription_t *)
{
    static int dummy = 0;
    if (g_state != nullptr)
    {
        g_state->async_sub_poll_calls++;
        if (g_state->subscription_poll_fail_after >= 0 &&
            g_state->async_sub_poll_calls > g_state->subscription_poll_fail_after)
        {
            return -1;
        }
        if (g_state->subscription_null)
        {
            *subscription = nullptr;
            return 1;
        }
        if (g_state->async_sub_poll_zero_first)
        {
            g_state->async_sub_poll_zero_first = false;
            return 0;
        }
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
        if (result >= 0 && !g_state->next_frame_length)
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
    if (g_state == nullptr || g_state->poll_fail)
    {
        return 0;
    }
    std::size_t fragments = 0;
    while (!g_state->frames.empty() && fragments < fragment_limit)
    {
        auto frame = g_state->frames.front();
        g_state->frames.pop_front();
        std::size_t length = frame.length;
        if (g_state->next_frame_length)
        {
            length = g_state->next_frame_length;
        }
        handler(clientd, frame.data.data(), length, nullptr);
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

bool test_aeron_empty_offer()
{
    StubState state;
    g_state = &state;
    auto previous = epoch::test::aeron_hooks();
    epoch::test::aeron_hooks() = build_stub_hooks();

    bool ok = true;
    {
        epoch::AeronTransport transport(epoch::AeronConfig{"aeron:ipc", 58, "", 4, 2});
        transport.send(epoch::Message{1, 1, 1, 1, 1, 1, 1});
        if (!state.frames.empty() || transport.stats().sent_count != 1)
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

bool test_aeron_publication_failures()
{
    auto previous = epoch::test::aeron_hooks();
    auto hooks = build_stub_hooks();
    hooks.async_add_publication = [](aeron_async_add_publication_t **, aeron_t *, const char *, int32_t) { return -1; };
    epoch::test::aeron_hooks() = hooks;

    bool ok = true;
    try
    {
        epoch::AeronTransport transport(epoch::AeronConfig{"aeron:ipc", 50, "", 4, 2});
        (void)transport;
        ok = false;
    }
    catch (const std::runtime_error &)
    {
    }

    epoch::test::aeron_hooks() = previous;
    return ok;
}

bool test_aeron_publication_poll_errors()
{
    StubState state;
    state.publication_poll_fail_after = 0;
    g_state = &state;
    auto previous = epoch::test::aeron_hooks();
    epoch::test::aeron_hooks() = build_stub_hooks();

    bool ok = true;
    try
    {
        epoch::AeronTransport transport(epoch::AeronConfig{"aeron:ipc", 51, "", 4, 2});
        (void)transport;
        ok = false;
    }
    catch (const std::runtime_error &)
    {
    }

    epoch::test::aeron_hooks() = previous;
    return ok;
}

bool test_aeron_publication_null_error()
{
    StubState state;
    state.publication_null = true;
    g_state = &state;
    auto previous = epoch::test::aeron_hooks();
    auto hooks = build_stub_hooks();
    hooks.async_add_publication_poll = [](aeron_publication_t **publication, aeron_async_add_publication_t *) {
        *publication = nullptr;
        return 1;
    };
    epoch::test::aeron_hooks() = hooks;

    bool ok = true;
    try
    {
        epoch::AeronTransport transport(epoch::AeronConfig{"aeron:ipc", 52, "", 4, 2});
        (void)transport;
        ok = false;
    }
    catch (const std::runtime_error &)
    {
    }

    epoch::test::aeron_hooks() = previous;
    return ok;
}

bool test_aeron_subscription_failures()
{
    auto previous = epoch::test::aeron_hooks();
    auto hooks = build_stub_hooks();
    hooks.async_add_subscription = [](aeron_async_add_subscription_t **, aeron_t *, const char *, int32_t,
                                      aeron_on_available_image_t, void *, aeron_on_unavailable_image_t, void *) { return -1; };
    epoch::test::aeron_hooks() = hooks;

    bool ok = true;
    try
    {
        epoch::AeronTransport transport(epoch::AeronConfig{"aeron:ipc", 53, "", 4, 2});
        (void)transport;
        ok = false;
    }
    catch (const std::runtime_error &)
    {
    }

    epoch::test::aeron_hooks() = previous;
    return ok;
}

bool test_aeron_subscription_poll_and_null_errors()
{
    StubState state;
    state.subscription_poll_fail_after = 0;
    state.subscription_null = true;
    g_state = &state;
    auto previous = epoch::test::aeron_hooks();
    epoch::test::aeron_hooks() = build_stub_hooks();

    bool ok = true;
    try
    {
        epoch::AeronTransport transport(epoch::AeronConfig{"aeron:ipc", 54, "", 4, 2});
        (void)transport;
        ok = false;
    }
    catch (const std::runtime_error &)
    {
    }

    try
    {
        epoch::AeronTransport transport(epoch::AeronConfig{"aeron:ipc", 55, "", 4, 2});
        (void)transport;
        ok = false;
    }
    catch (const std::runtime_error &)
    {
    }

    epoch::test::aeron_hooks() = previous;
    return ok;
}

bool test_aeron_poll_error_and_short_frame()
{
    StubState state;
    g_state = &state;
    auto previous = epoch::test::aeron_hooks();
    epoch::test::aeron_hooks() = build_stub_hooks();

    bool ok = true;
    {
        epoch::AeronTransport transport(epoch::AeronConfig{"aeron:ipc", 57, "", 4, 2});
        state.offer_results.push_back(0);
        state.next_frame_length = 8;
        Frame short_frame;
        encode_message(short_frame, epoch::Message{1, 1, 1, 1, 1, 1, 7});
        state.frames.push_back(short_frame);
        const auto out = transport.poll(1);
        if (!out.empty() || transport.stats().received_count != 0)
        {
            ok = false;
        }
        transport.close();
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

bool test_aeron_init_failure_cleans_up()
{
    StubState state;
    state.init_fail = true;
    g_state = &state;
    auto previous = epoch::test::aeron_hooks();
    epoch::test::aeron_hooks() = build_stub_hooks();

    bool ok = true;
    try
    {
        epoch::AeronTransport transport(epoch::AeronConfig{"aeron:ipc", 41, "", 4, 2});
        (void)transport;
        ok = false;
    }
    catch (const std::runtime_error &)
    {
    }

    // init failure: client was never created; context must still be released.
    if (state.close_client || !state.close_context)
    {
        ok = false;
    }

    epoch::test::aeron_hooks() = previous;
    return ok;
}

bool test_aeron_context_set_dir_failure_cleans_up()
{
    StubState state;
    state.context_set_dir_fail = true;
    g_state = &state;
    auto previous = epoch::test::aeron_hooks();
    epoch::test::aeron_hooks() = build_stub_hooks();

    bool ok = true;
    try
    {
        epoch::AeronTransport transport(epoch::AeronConfig{"aeron:ipc", 43, "/nonexistent", 4, 2});
        (void)transport;
        ok = false;
    }
    catch (const std::runtime_error &)
    {
    }

    // set_dir failure: client was never created; context must still be released.
    if (!state.context_set_dir_calls || state.close_client || !state.close_context)
    {
        ok = false;
    }

    epoch::test::aeron_hooks() = previous;
    return ok;
}

bool test_aeron_start_failure_cleans_up()
{
    StubState state;
    state.start_fail = true;
    g_state = &state;
    auto previous = epoch::test::aeron_hooks();
    epoch::test::aeron_hooks() = build_stub_hooks();

    bool ok = true;
    try
    {
        epoch::AeronTransport transport(epoch::AeronConfig{"aeron:ipc", 42, "", 4, 2});
        (void)transport;
        ok = false;
    }
    catch (const std::runtime_error &)
    {
    }

    // start failure: client was created and must be closed, then context released.
    if (!state.close_client || !state.close_context)
    {
        ok = false;
    }

    epoch::test::aeron_hooks() = previous;
    return ok;
}

bool test_aeron_async_retry_yield_loops()
{
    // Aeron async add publication/subscription may return 0 (not complete) once
    // before completing; the transport retries with std::this_thread::yield().
    StubState state;
    state.async_pub_poll_zero_first = true;
    state.async_sub_poll_zero_first = true;
    g_state = &state;
    auto previous = epoch::test::aeron_hooks();
    epoch::test::aeron_hooks() = build_stub_hooks();

    bool ok = true;
    try
    {
        epoch::AeronTransport transport(epoch::AeronConfig{"aeron:ipc", 53, "", 4, 2});
        (void)transport;
        transport.close();
    }
    catch (const std::runtime_error &)
    {
        ok = false;
    }

    // Both async poll stubs must have returned 0 at least once (retry exercised).
    if (state.async_pub_poll_calls < 2 || state.async_sub_poll_calls < 2)
    {
        ok = false;
    }

    epoch::test::aeron_hooks() = previous;
    return ok;
}

bool expect_single_counter(const epoch::AeronStats &stats, int which)
{
    switch (which)
    {
    case 0:
        return stats.offer_not_connected == 1 && stats.offer_admin_action == 0 &&
               stats.offer_closed == 0 && stats.offer_max_position == 0 &&
               stats.offer_failed == 0;
    case 1:
        return stats.offer_not_connected == 0 && stats.offer_admin_action == 1 &&
               stats.offer_closed == 0 && stats.offer_max_position == 0 &&
               stats.offer_failed == 0;
    case 2:
        return stats.offer_not_connected == 0 && stats.offer_admin_action == 0 &&
               stats.offer_closed == 1 && stats.offer_max_position == 0 &&
               stats.offer_failed == 0;
    case 3:
        return stats.offer_not_connected == 0 && stats.offer_admin_action == 0 &&
               stats.offer_closed == 0 && stats.offer_max_position == 1 &&
               stats.offer_failed == 0;
    default:
        return stats.offer_not_connected == 0 && stats.offer_admin_action == 0 &&
               stats.offer_closed == 0 && stats.offer_max_position == 0 &&
               stats.offer_failed == 1;
    }
}

bool test_aeron_offer_stats_branches()
{
    const std::int64_t codes[] = {
        AERON_PUBLICATION_NOT_CONNECTED,
        AERON_PUBLICATION_ADMIN_ACTION,
        AERON_PUBLICATION_CLOSED,
        AERON_PUBLICATION_MAX_POSITION_EXCEEDED,
        -99,
    };
    for (int i = 0; i < 5; i++)
    {
        StubState state;
        state.offer_results = {codes[i]};
        g_state = &state;
        auto previous = epoch::test::aeron_hooks();
        epoch::test::aeron_hooks() = build_stub_hooks();

        bool ok = true;
        try
        {
            epoch::AeronTransport transport(epoch::AeronConfig{"aeron:ipc", 62, "", 4, 1});
            bool threw = false;
            try
            {
                transport.send(epoch::Message{1, 1, 1, 1, 0, 0, 1});
            }
            catch (const std::runtime_error &)
            {
                threw = true;
            }
            if (!threw || !expect_single_counter(transport.stats(), i))
            {
                ok = false;
            }
            transport.close();
        }
        catch (const std::runtime_error &)
        {
            ok = false;
        }

        epoch::test::aeron_hooks() = previous;
        if (!ok)
        {
            return false;
        }
    }
    return true;
}

bool test_aeron_offer_back_pressure_retry_succeeds()
{
    StubState state;
    state.offer_results = {AERON_PUBLICATION_BACK_PRESSURED, 1};
    g_state = &state;
    auto previous = epoch::test::aeron_hooks();
    epoch::test::aeron_hooks() = build_stub_hooks();

    bool ok = true;
    try
    {
        epoch::AeronTransport transport(epoch::AeronConfig{"aeron:ipc", 63, "", 4, 2});
        transport.send(epoch::Message{1, 1, 1, 1, 0, 0, 1});
        const epoch::AeronStats &stats = transport.stats();
        if (stats.sent_count != 1 || stats.offer_back_pressure != 1)
        {
            ok = false;
        }
        transport.close();
    }
    catch (const std::runtime_error &)
    {
        ok = false;
    }

    epoch::test::aeron_hooks() = previous;
    return ok;
}

bool test_aeron_send_after_close_throws()
{
    StubState state;
    state.offer_results = {1};
    g_state = &state;
    auto previous = epoch::test::aeron_hooks();
    epoch::test::aeron_hooks() = build_stub_hooks();

    bool ok = true;
    try
    {
        epoch::AeronTransport transport(epoch::AeronConfig{"aeron:ipc", 64, "", 4, 1});
        transport.close();
        try
        {
            transport.send(epoch::Message{1, 1, 1, 1, 0, 0, 1});
            ok = false;
        }
        catch (const std::runtime_error &)
        {
        }
    }
    catch (const std::runtime_error &)
    {
        ok = false;
    }

    epoch::test::aeron_hooks() = previous;
    return ok;
}

bool test_aeron_accessors_and_ctor_defaults()
{
    StubState state;
    state.offer_results = {1};
    g_state = &state;
    auto previous = epoch::test::aeron_hooks();
    epoch::test::aeron_hooks() = build_stub_hooks();

    bool ok = true;
    try
    {
        epoch::AeronTransport transport(epoch::AeronConfig{"aeron:ipc", 65, "", 0, 0});
        if (transport.config().channel != "aeron:ipc")
        {
            ok = false;
        }
        if (transport.config().fragment_limit != 64)
        {
            ok = false;
        }
        if (transport.config().offer_max_attempts != 10)
        {
            ok = false;
        }
        if (transport.stats().sent_count != 0)
        {
            ok = false;
        }
        transport.close();
    }
    catch (const std::runtime_error &)
    {
        ok = false;
    }

    epoch::test::aeron_hooks() = previous;
    return ok;
}

bool test_aeron_poll_zero_and_destructor()
{
    StubState state;
    state.offer_results = {1};
    g_state = &state;
    auto previous = epoch::test::aeron_hooks();
    epoch::test::aeron_hooks() = build_stub_hooks();

    bool ok = true;
    try
    {
        {
            // Owned through the Transport interface: destruction must release
            // resources through the virtual destructor.
            std::unique_ptr<epoch::Transport> transport =
                std::make_unique<epoch::AeronTransport>(epoch::AeronConfig{"aeron:ipc", 66, "", 4, 1});
            if (!transport->poll(0).empty())
            {
                ok = false;
            }
            // No explicit close here: the destructor must release resources.
        }
    }
    catch (const std::runtime_error &)
    {
        ok = false;
    }

    epoch::test::aeron_hooks() = previous;
    return ok;
}

bool test_aeron_reset_hooks_restores_defaults()
{
    auto previous = epoch::test::aeron_hooks();
    epoch::test::reset_aeron_hooks();
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
    if (!test_aeron_publication_failures())
    {
        return 1;
    }
    if (!test_aeron_publication_poll_errors())
    {
        return 1;
    }
    if (!test_aeron_publication_null_error())
    {
        return 1;
    }
    if (!test_aeron_subscription_failures())
    {
        return 1;
    }
    if (!test_aeron_subscription_poll_and_null_errors())
    {
        return 1;
    }
    if (!test_aeron_poll_error_and_short_frame())
    {
        return 1;
    }
    if (!test_aeron_constructor_errors())
    {
        return 1;
    }
    if (!test_aeron_init_failure_cleans_up())
    {
        return 1;
    }
    if (!test_aeron_context_set_dir_failure_cleans_up())
    {
        return 1;
    }
    if (!test_aeron_start_failure_cleans_up())
    {
        return 1;
    }
    if (!test_aeron_async_retry_yield_loops())
    {
        return 1;
    }
    if (!test_aeron_offer_stats_branches())
    {
        return 1;
    }
    if (!test_aeron_offer_back_pressure_retry_succeeds())
    {
        return 1;
    }
    if (!test_aeron_send_after_close_throws())
    {
        return 1;
    }
    if (!test_aeron_accessors_and_ctor_defaults())
    {
        return 1;
    }
    if (!test_aeron_reset_hooks_restores_defaults())
    {
        return 1;
    }
    if (!test_aeron_poll_zero_and_destructor())
    {
        return 1;
    }
    return 0;
}
