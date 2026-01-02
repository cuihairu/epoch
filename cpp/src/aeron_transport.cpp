#include "epoch/aeron_transport.h"

#include <stdexcept>

namespace epoch {

AeronTransport::AeronTransport(AeronConfig config) : config_(std::move(config)) {}

void AeronTransport::send(const Message &)
{
    throw std::runtime_error("Aeron transport not linked");
}

std::vector<Message> AeronTransport::poll(std::size_t)
{
    throw std::runtime_error("Aeron transport not linked");
}

void AeronTransport::close()
{
    closed_ = true;
}

} // namespace epoch
