#include "client/LocalSpawn.hpp"

#include <string>

#include "RuntimeContext.hpp"

namespace engine::script {

void bind_spawn_local(RuntimeContext& context) {
    context.lua.set_function("spawn_local",
                             [&context](sol::this_state state, const std::string& type,
                                        const sol::optional<sol::table>& overrides) {
                                 return context.spawn_declared(state, type, overrides);
                             });
}

} // namespace engine::script
