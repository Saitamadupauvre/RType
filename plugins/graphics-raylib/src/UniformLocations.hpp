#pragma once

#include <functional>
#include <string>
#include <unordered_map>

class UniformLocations {
public:
    static constexpr int missing = -1;

    using Lookup = std::function<int(const std::string&)>;

    int find(const std::string& name, const Lookup& lookup) {
        if (const auto found = _locations.find(name); found != _locations.end()) {
            return found->second;
        }
        const int location = lookup(name);
        _locations.emplace(name, location < 0 ? missing : location);
        return location < 0 ? missing : location;
    }

private:
    std::unordered_map<std::string, int> _locations;
};
