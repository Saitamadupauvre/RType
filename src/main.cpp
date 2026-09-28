#include <iostream>

#include "rtype/rtype.hpp"
#include "rtype/version.hpp"

int main() {
    std::cout << rtype::greet("RType") << " (v" << rtype::version << ")\n";
    return 0;
}
