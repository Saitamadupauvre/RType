#include "engine/network/Transport.hpp"

#include <algorithm>
#include <utility>
#include <vector>

#include "transport/Link.hpp"
#include "transport/LoopbackLinks.hpp"
#include "transport/UdpLinks.hpp"

namespace engine::network {

namespace {

class LoopbackServerTransport final : public ServerTransport {
public:
    explicit LoopbackServerTransport(LoopbackLink& link) : _link(link) {}

private:
    [[nodiscard]] Link& link() noexcept override { return _link; }

    LoopbackLink& _link;
};

class LoopbackClientTransport final : public ClientTransport {
public:
    explicit LoopbackClientTransport(LoopbackLink& link) : _link(link) {}

    [[nodiscard]] PeerId peer() const noexcept { return _link.self(); }

private:
    [[nodiscard]] Link& link() noexcept override { return _link; }

    LoopbackLink& _link;
};

} // namespace

Link::~Link() = default;

ServerTransport::~ServerTransport() = default;

ClientTransport::~ClientTransport() = default;

struct UdpServerTransport::Impl {
    explicit Impl(std::uint16_t port) : link(port) {}

    UdpServerLink link;
};

UdpServerTransport::UdpServerTransport(std::uint16_t port) : _impl(std::make_unique<Impl>(port)) {}

UdpServerTransport::~UdpServerTransport() = default;

std::uint16_t UdpServerTransport::port() const { return _impl->link.port(); }

Link& UdpServerTransport::link() noexcept { return _impl->link; }

struct UdpClientTransport::Impl {
    Impl(std::string server_ip, std::uint16_t port) : link(std::move(server_ip), port) {}

    UdpClientLink link;
};

UdpClientTransport::UdpClientTransport(std::string server_ip, std::uint16_t port)
    : _impl(std::make_unique<Impl>(std::move(server_ip), port)) {}

UdpClientTransport::~UdpClientTransport() = default;

Link& UdpClientTransport::link() noexcept { return _impl->link; }

struct LoopbackNetwork::Impl {
    LoopbackHub hub;
    LoopbackServerTransport server{hub.server()};
    std::vector<std::unique_ptr<LoopbackClientTransport>> clients;
};

LoopbackNetwork::LoopbackNetwork() : _impl(std::make_unique<Impl>()) {}

LoopbackNetwork::~LoopbackNetwork() = default;

ServerTransport& LoopbackNetwork::server() noexcept { return _impl->server; }

ClientTransport& LoopbackNetwork::add_client() {
    auto& link = _impl->hub.add_client();
    return *_impl->clients.emplace_back(std::make_unique<LoopbackClientTransport>(link));
}

void LoopbackNetwork::cut(const ClientTransport& client) {
    const auto found = std::ranges::find_if(
        _impl->clients, [&client](const auto& owned) { return owned.get() == &client; });
    if (found != _impl->clients.end()) {
        _impl->hub.cut((*found)->peer());
    }
}

} // namespace engine::network
