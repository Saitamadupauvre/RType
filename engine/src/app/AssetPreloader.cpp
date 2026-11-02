#include "AssetPreloader.hpp"

#include <utility>

#include "engine/core/log/Log.hpp"

namespace engine::app {

void AssetPreloader::add(Load load) { _loads.push_back(std::move(load)); }

void AssetPreloader::load_next(std::size_t count) {
    for (std::size_t loads = 0; loads < count && !done(); ++loads) {
        load_one();
    }
}

void AssetPreloader::load_one() {
    if (!_loads[_loaded]()) {
        ++_missing;
    }
    ++_loaded;
    if (done()) {
        core::log::info("client: preloaded {} assets, {} missing", total(), _missing);
    }
}

bool AssetPreloader::done() const noexcept { return _loaded == _loads.size(); }

std::size_t AssetPreloader::loaded() const noexcept { return _loaded; }

std::size_t AssetPreloader::total() const noexcept { return _loads.size(); }

std::size_t AssetPreloader::missing() const noexcept { return _missing; }

} // namespace engine::app
