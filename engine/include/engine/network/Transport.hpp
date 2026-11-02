#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "engine/network/Export.hpp"

namespace engine::network {

class Link;
class NetworkServer;
class NetworkClient;

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4251)
#endif

/**
 * @brief Carries the packets of one NetworkServer to and from its clients.
 *
 * The server opens it in NetworkServer::start() and closes it in NetworkServer::stop(). The
 * engine provides UdpServerTransport and the server side of a LoopbackNetwork; whichever is
 * used, the server behaves the same.
 *
 * @note Must outlive the server using it. One transport serves one server at a time.
 */
class ENGINE_NETWORK_EXPORT ServerTransport {
public:
    virtual ~ServerTransport();

    ServerTransport(const ServerTransport&) = delete;
    ServerTransport& operator=(const ServerTransport&) = delete;
    ServerTransport(ServerTransport&&) = delete;
    ServerTransport& operator=(ServerTransport&&) = delete;

protected:
    ServerTransport() = default;

private:
    friend class NetworkServer;

    [[nodiscard]] virtual Link& link() noexcept = 0;
};

/**
 * @brief Carries the packets of one NetworkClient to and from its server.
 *
 * The client opens it in NetworkClient::connect() and closes it when the connection ends. The
 * engine provides UdpClientTransport and the clients of a LoopbackNetwork; whichever is used,
 * the client behaves the same.
 *
 * @note Must outlive the client using it. One transport serves one client at a time.
 */
class ENGINE_NETWORK_EXPORT ClientTransport {
public:
    virtual ~ClientTransport();

    ClientTransport(const ClientTransport&) = delete;
    ClientTransport& operator=(const ClientTransport&) = delete;
    ClientTransport(ClientTransport&&) = delete;
    ClientTransport& operator=(ClientTransport&&) = delete;

protected:
    ClientTransport() = default;

private:
    friend class NetworkClient;

    [[nodiscard]] virtual Link& link() noexcept = 0;
};

/**
 * @brief Server transport over UDP, listening on every IPv4 interface.
 *
 * Two background threads read and write the socket and decode the packets.
 */
class ENGINE_NETWORK_EXPORT UdpServerTransport final : public ServerTransport {
public:
    /** @param port UDP port to listen on. `0` picks a free port when the server starts. */
    explicit UdpServerTransport(std::uint16_t port);
    ~UdpServerTransport() override;

    UdpServerTransport(const UdpServerTransport&) = delete;
    UdpServerTransport& operator=(const UdpServerTransport&) = delete;
    UdpServerTransport(UdpServerTransport&&) = delete;
    UdpServerTransport& operator=(UdpServerTransport&&) = delete;

    /**
     * @return The bound port while open (useful with port `0`), otherwise the requested one.
     */
    [[nodiscard]] std::uint16_t port() const;

private:
    [[nodiscard]] Link& link() noexcept override;

    struct Impl;
    std::unique_ptr<Impl> _impl;
};

/**
 * @brief Client transport over UDP, sending from a free local port to one server.
 *
 * Packets from any other address are ignored. Two background threads read and write the socket
 * and decode the packets.
 */
class ENGINE_NETWORK_EXPORT UdpClientTransport final : public ClientTransport {
public:
    /**
     * @param server_ip Numeric IPv4 or IPv6 address of the server, e.g. `127.0.0.1`. Checked
     * when the client connects: NetworkClient::connect() fails with
     * `std::errc::invalid_argument` if it is not a numeric address.
     * @param port UDP port of the server.
     */
    UdpClientTransport(std::string server_ip, std::uint16_t port);
    ~UdpClientTransport() override;

    UdpClientTransport(const UdpClientTransport&) = delete;
    UdpClientTransport& operator=(const UdpClientTransport&) = delete;
    UdpClientTransport(UdpClientTransport&&) = delete;
    UdpClientTransport& operator=(UdpClientTransport&&) = delete;

private:
    [[nodiscard]] Link& link() noexcept override;

    struct Impl;
    std::unique_ptr<Impl> _impl;
};

/**
 * @brief In-memory network linking one server and any number of clients in the same process.
 *
 * Packets are encoded and decoded exactly as over UDP, but travel through in-memory queues:
 * a packet sent is received by the next NetworkServer::receive() or NetworkClient::update() of
 * its destination, in order, with no loss. Packets to or from a transport that is not open are
 * dropped, like UDP packets to a closed port. No thread is started.
 *
 * @note Thread-safe: the server and the clients may run on different threads. Must outlive
 * every transport it gave out.
 */
class ENGINE_NETWORK_EXPORT LoopbackNetwork {
public:
    LoopbackNetwork();
    ~LoopbackNetwork();

    LoopbackNetwork(const LoopbackNetwork&) = delete;
    LoopbackNetwork& operator=(const LoopbackNetwork&) = delete;
    LoopbackNetwork(LoopbackNetwork&&) = delete;
    LoopbackNetwork& operator=(LoopbackNetwork&&) = delete;

    /** @return The transport of the server; always the same one. */
    [[nodiscard]] ServerTransport& server() noexcept;

    /** @return A new client transport, linked to the server. */
    [[nodiscard]] ClientTransport& add_client();

    /**
     * @brief Silently drops every packet to and from @p client from now on, as when its cable
     * is pulled. Both ends then time out.
     *
     * @param client A transport returned by add_client(); anything else is ignored.
     */
    void cut(const ClientTransport& client);

private:
    struct Impl;
    std::unique_ptr<Impl> _impl;
};

#ifdef _MSC_VER
#pragma warning(pop)
#endif

} // namespace engine::network
