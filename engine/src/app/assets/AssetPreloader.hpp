#pragma once

#include <cstddef>
#include <functional>
#include <vector>

#include "engine/app/Export.hpp"

namespace engine::app {

class ENGINE_APP_EXPORT AssetPreloader {
public:
    using Load = std::function<bool()>;

    void add(Load load);
    void load_next(std::size_t count);

    [[nodiscard]] bool done() const noexcept;
    [[nodiscard]] std::size_t loaded() const noexcept;
    [[nodiscard]] std::size_t total() const noexcept;
    [[nodiscard]] std::size_t missing() const noexcept;

private:
    void load_one();

    std::vector<Load> _loads;
    std::size_t _loaded{0};
    std::size_t _missing{0};
};

} // namespace engine::app
