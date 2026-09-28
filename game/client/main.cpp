#include <iostream>

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <server-ip> <port>\n";
        return 84;
    }
    return 0;
}
