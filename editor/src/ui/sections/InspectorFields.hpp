#pragma once

#include <cstdint>
#include <string_view>

#include "action/Action.hpp"
#include "action/InspectorModel.hpp"
#include "ui/Ui.hpp"

namespace editor {

inline constexpr float reset_width = 72.F;

struct FieldRow {
    InstanceId id;
    const InspectorField& field;
    std::uint32_t index = 0;
    std::string_view label;
};

void field_row(Ui& ui, const FieldRow& row);

} // namespace editor
