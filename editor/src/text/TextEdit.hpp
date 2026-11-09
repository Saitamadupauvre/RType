#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>

namespace editor {

enum class Motion : std::uint8_t { left, right, word_left, word_right, home, end };

class TextEdit {
public:
    explicit TextEdit(std::string text = {});

    [[nodiscard]] const std::string& text() const noexcept;
    [[nodiscard]] std::size_t caret() const noexcept;
    [[nodiscard]] std::size_t anchor() const noexcept;
    [[nodiscard]] bool has_selection() const noexcept;
    [[nodiscard]] std::string_view selection() const;

    void insert(std::string_view typed);
    void erase_backward();
    void erase_forward();
    void erase_word_backward();
    void erase_word_forward();
    void move(Motion motion, bool extend);
    void place_caret(std::size_t index, bool extend);
    void select_all();
    void select_word();

private:
    [[nodiscard]] std::size_t target_of(Motion motion) const;
    void erase_selection();

    std::string _text;
    std::size_t _caret = 0;
    std::size_t _anchor = 0;
};

using MeasureWidth = std::function<float(std::string_view prefix)>;

[[nodiscard]] std::size_t caret_at(std::string_view text, float x, const MeasureWidth& measure);

} // namespace editor
