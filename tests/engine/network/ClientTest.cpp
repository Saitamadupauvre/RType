#include <gtest/gtest.h>

#include "engine/network/Client.hpp"

TEST(NetworkClient, StoresServerEndpoint) {
    const engine::network::NetworkClient client("127.0.0.1", 4242);

    EXPECT_EQ(client.server_address(), "127.0.0.1");
    EXPECT_EQ(client.port(), 4242);
}
