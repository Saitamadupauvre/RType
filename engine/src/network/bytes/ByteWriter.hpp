#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

namespace engine::network {

class ByteWriter {
public:
    void write_u8(std::uint8_t value) { write_big_endian(value, 1); }

    void write_u16(std::uint16_t value) { write_big_endian(value, 2); }

    void write_u32(std::uint32_t value) { write_big_endian(value, 4); }

    void write_i8(std::int8_t value) { write_u8(static_cast<std::uint8_t>(value)); }

    void write_i16(std::int16_t value) { write_u16(static_cast<std::uint16_t>(value)); }

    void write_bytes(std::span<const std::uint8_t> bytes) {
        bytes_.insert(bytes_.end(), bytes.begin(), bytes.end());
    }

    [[nodiscard]] std::size_t size() const noexcept { return bytes_.size(); }

    [[nodiscard]] const std::vector<std::uint8_t>& bytes() const& noexcept { return bytes_; }

    [[nodiscard]] std::vector<std::uint8_t> take() && noexcept { return std::move(bytes_); }

private:
    void write_big_endian(std::uint32_t value, std::size_t width) {
        for (std::size_t index = width; index > 0; --index) {
            bytes_.push_back(static_cast<std::uint8_t>(value >> ((index - 1) * 8U)));
        }
    }

    std::vector<std::uint8_t> bytes_;
};

} // namespace engine::network
