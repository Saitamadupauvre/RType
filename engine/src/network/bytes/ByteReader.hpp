#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace engine::network {

class ByteReader {
public:
    explicit ByteReader(std::span<const std::uint8_t> data) noexcept : data_(data) {}

    [[nodiscard]] std::optional<std::uint8_t> read_u8() noexcept {
        return narrow<std::uint8_t>(read_big_endian(1));
    }

    [[nodiscard]] std::optional<std::uint16_t> read_u16() noexcept {
        return narrow<std::uint16_t>(read_big_endian(2));
    }

    [[nodiscard]] std::optional<std::uint32_t> read_u32() noexcept { return read_big_endian(4); }

    [[nodiscard]] std::optional<std::int8_t> read_i8() noexcept {
        return narrow<std::int8_t>(read_big_endian(1));
    }

    [[nodiscard]] std::optional<std::int16_t> read_i16() noexcept {
        return narrow<std::int16_t>(read_big_endian(2));
    }

    [[nodiscard]] std::optional<std::span<const std::uint8_t>>
    read_bytes(std::size_t count) noexcept {
        if (count > remaining()) {
            return std::nullopt;
        }
        const auto bytes = data_.subspan(offset_, count);
        offset_ += count;
        return bytes;
    }

    [[nodiscard]] std::size_t remaining() const noexcept { return data_.size() - offset_; }

    [[nodiscard]] bool at_end() const noexcept { return remaining() == 0; }

private:
    template <typename T>
    static std::optional<T> narrow(std::optional<std::uint32_t> value) noexcept {
        if (!value.has_value()) {
            return std::nullopt;
        }
        return static_cast<T>(*value);
    }

    std::optional<std::uint32_t> read_big_endian(std::size_t width) noexcept {
        const auto bytes = read_bytes(width);
        if (!bytes.has_value()) {
            return std::nullopt;
        }
        std::uint32_t value = 0;
        for (const std::uint8_t byte : *bytes) {
            value = (value << 8U) | byte;
        }
        return value;
    }

    std::span<const std::uint8_t> data_;
    std::size_t offset_{0};
};

} // namespace engine::network
