#pragma once

#include "epoch/engine.h"

#include <cstddef>
#include <deque>
#include <vector>

namespace epoch {

class Transport {
public:
    virtual ~Transport() = default;
    virtual void send(const Message &message) = 0;
    virtual std::vector<Message> poll(std::size_t max) = 0;
    virtual void close() = 0;
};

class InMemoryTransport final : public Transport {
public:
    void send(const Message &message) override
    {
        queue_.push_back(message);
    }

    std::vector<Message> poll(std::size_t max) override
    {
        std::vector<Message> out;
        while (!queue_.empty() && out.size() < max)
        {
            out.push_back(queue_.front());
            queue_.pop_front();
        }
        return out;
    }

    void close() override
    {
        queue_.clear();
    }

private:
    std::deque<Message> queue_;
};

} // namespace epoch
