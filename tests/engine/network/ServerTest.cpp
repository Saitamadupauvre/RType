#include <gtest/gtest.h>

#include "engine/network/Server.hpp"

TEST(NetworkServer, StoresListeningPort) {
    const engine::network::NetworkServer server(4242);

    EXPECT_EQ(server.port(), 4242);
}
