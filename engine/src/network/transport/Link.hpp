#pragma once

#include <string>
#include <system_error>
#include <vector>

#include "engine/network/Export.hpp"

#include "thread/NetworkMessages.hpp"

namespace engine::network {

class ENGINE_NETWORK_EXPORT Link {
public:
    Link() = default;
    virtual ~Link();

    Link(const Link&) = delete;
    Link& operator=(const Link&) = delete;
    Link(Link&&) = delete;
    Link& operator=(Link&&) = delete;

    virtual void open(std::error_code& ec) = 0;
    virtual void close() = 0;
    [[nodiscard]] virtual std::vector<Inbound> poll() = 0;
    virtual void send(Outbound outbound) = 0;
    virtual void forget(PeerId peer) = 0;
    [[nodiscard]] virtual std::string describe(PeerId peer) const = 0;
};

} // namespace engine::network
