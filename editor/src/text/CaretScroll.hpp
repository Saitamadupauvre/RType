#pragma once

namespace editor {

/**
 * @brief Scrolls a one-line field just enough for the caret to stay visible.
 *
 * @param offset Current scroll, in pixels from the start of the text.
 * @param caret_x Distance from the start of the text to the caret.
 * @param text_width Width of the whole text.
 * @param visible Width of the field's text area.
 * @return The new scroll: unchanged while the caret is visible, never past the end of the text,
 * never negative.
 */
[[nodiscard]] float scroll_to_caret(float offset, float caret_x, float text_width,
                                    float visible) noexcept;

} // namespace editor
