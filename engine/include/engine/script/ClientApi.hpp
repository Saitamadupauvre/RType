#pragma once

#include <memory>
#include <string_view>

#include "engine/script/ApiSet.hpp"
#include "engine/script/EventCommand.hpp"
#include "engine/script/Export.hpp"

namespace engine::script {

class ScriptRuntimeBuilder;

/**
 * @brief API set of the presentation side: reacting to events received from the server.
 *
 * Adds the Lua global `on(name, handler)` and reads the `client = { ... }` section of
 * declarations. `on` takes an event declared with `event "name" { ... }` and a function; the
 * function is called with a table of the event's arguments every time dispatch() receives that
 * event. An undeclared event or a handler that is not a function raises a Lua error.
 *
 * @note Not thread safe. Created by ScriptRuntimeBuilder::with(), reached with
 * ScriptRuntime::api(), destroyed with its runtime.
 */
class ENGINE_SCRIPT_EXPORT ClientApi final : public ApiSet {
public:
    ~ClientApi() override;

    ClientApi(const ClientApi&) = delete;
    ClientApi& operator=(const ClientApi&) = delete;
    ClientApi(ClientApi&&) = delete;
    ClientApi& operator=(ClientApi&&) = delete;

    /** @brief Returns `"client"`. */
    [[nodiscard]] std::string_view section() const noexcept override;

    /**
     * @brief Calls every handler registered with `on(event.name, ...)`, in registration order.
     *
     * Each handler gets a new table mapping every argument key to its value, converted like an
     * entity field. Every call is protected: an error is logged and the other handlers still run.
     *
     * @param event Event received from the server, typically converted from a NamedEvent.
     * @return true if every handler ran to completion, including when there is none; false if a
     * handler raised an error.
     * @note Never throws.
     */
    bool dispatch(const EventCommand& event) noexcept;

private:
    friend class ScriptRuntimeBuilder;

    struct State;

    ClientApi();
    void install(RuntimeContext& context) override;

    std::unique_ptr<State> _state;
};

} // namespace engine::script
