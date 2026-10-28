#include "engine/script/ApiSet.hpp"

namespace engine::script {

ApiSet::~ApiSet() = default;

std::string_view ApiSet::section() const noexcept { return {}; }

} // namespace engine::script
