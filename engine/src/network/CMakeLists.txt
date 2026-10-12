engine_add_module(network
    SOURCES
        Client.cpp
        Module.cpp
        Protocol.cpp
        ProtocolHandler.cpp
        Server.cpp
        UdpSocket.cpp
    DEPENDS engine::core
)
