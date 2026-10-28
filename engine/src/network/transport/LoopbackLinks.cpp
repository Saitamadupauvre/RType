#include "LoopbackLinks.hpp"

#include <cstdint>
#include <utility>

#include "engine/core/log/Log.hpp"

namespace engine::network {

std::string loopback_name(PeerId peer) {
    if (peer == server_peer) {
        return "loopback server";
    }
    return "loopback client " + std::to_string(static_cast<std::uint32_t>(peer));
}

LoopbackLink::LoopbackLink(LoopbackHub& hub, PeerId self) : _hub(hub), _self(self) {}

void LoopbackLink::open(std::error_code& ec) {
    ec.clear();
    const std::scoped_lock lock(_hub._mutex);
    _open = true;
}

void LoopbackLink::close() {
    const std::scoped_lock lock(_hub._mutex);
    _open = false;
    _inbox.clear();
}

std::vector<Inbound> LoopbackLink::poll() {
    std::vector<Packet> packets;
    {
        const std::scoped_lock lock(_hub._mutex);
        packets.swap(_inbox);
    }
    std::vector<Inbound> inbound;
    inbound.reserve(packets.size());
    for (const auto& packet : packets) {
        auto received = _hub._protocol.decode(packet.bytes);
        if (!received) {
            core::log::warn("network: dropped invalid datagram of {} bytes from {}",
                            packet.bytes.size(), describe(packet.from));
            continue;
        }
        inbound.push_back(Inbound{.from = packet.from, .message = std::move(*received)});
    }
    return inbound;
}

void LoopbackLink::send(Outbound outbound) {
    auto bytes = _hub._protocol.encode(outbound.message, outbound.sequence);
    if (!bytes) {
        core::log::warn("network: dropped invalid outbound message to {}", describe(outbound.to));
        return;
    }
    _hub.deliver(_self, outbound.to, std::move(*bytes));
}

void LoopbackLink::forget(PeerId) {}

std::string LoopbackLink::describe(PeerId peer) const { return loopback_name(peer); }

LoopbackHub::LoopbackHub(std::size_t inbox_capacity)
    : _inbox_capacity(inbox_capacity), _server(std::make_unique<LoopbackLink>(*this, server_peer)) {
}

LoopbackLink& LoopbackHub::add_client() {
    const std::scoped_lock lock(_mutex);
    const PeerId peer{static_cast<std::uint32_t>(_clients.size() + 1)};
    return *_clients.emplace_back(std::make_unique<LoopbackLink>(*this, peer));
}

void LoopbackHub::cut(PeerId client) {
    const std::scoped_lock lock(_mutex);
    if (auto* link = find(client); link != nullptr && link != _server.get()) {
        link->_cut = true;
    }
}

LoopbackLink* LoopbackHub::find(PeerId peer) {
    if (peer == server_peer) {
        return _server.get();
    }
    const auto index = static_cast<std::size_t>(peer) - 1;
    return index < _clients.size() ? _clients[index].get() : nullptr;
}

void LoopbackHub::deliver(PeerId from, PeerId to, Bytes bytes) {
    const std::scoped_lock lock(_mutex);
    auto* sender = find(from);
    auto* target = find(to);
    if (sender == nullptr || target == nullptr || sender == target || !sender->_open ||
        !target->_open || sender->_cut || target->_cut) {
        return;
    }
    if (target->_inbox.size() >= _inbox_capacity) {
        core::log::warn("network: inbox full, dropped message from {}", loopback_name(from));
        return;
    }
    target->_inbox.push_back(LoopbackLink::Packet{.from = from, .bytes = std::move(bytes)});
}

} // namespace engine::network
