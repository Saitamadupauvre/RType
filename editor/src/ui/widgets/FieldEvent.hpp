#pragma once

#include <cstdint>
#include <string>

namespace editor {

/** @brief What happened to a field this frame. */
enum class FieldEvent : std::uint8_t {
    none,
    /** The draft changed; the value is not written yet. */
    changed,
    /** Enter, Tab or a click outside accepted the draft: the caller emits its action. */
    committed,
    /** Esc restored the value the field had when it took focus. */
    cancelled,
    /** The draft was committed but is not a valid value: the field shows the value again. */
    rejected,
};

/** @brief Result of a field: the event and the value it carries. */
template <typename Value> struct FieldResult {
    FieldEvent event = FieldEvent::none;
    Value value{};
};

using TextResult = FieldResult<std::string>;

} // namespace editor
