#pragma once

#include "engine/graphics/IInput.hpp"
#include "engine/graphics/Types.hpp"

class RaylibInput final : public engine::graphics::IInput {
public:
    [[nodiscard]] bool is_key_down(engine::graphics::Key key) const override;
    [[nodiscard]] bool is_key_pressed(engine::graphics::Key key) const override;
    [[nodiscard]] bool is_key_released(engine::graphics::Key key) const override;
};
