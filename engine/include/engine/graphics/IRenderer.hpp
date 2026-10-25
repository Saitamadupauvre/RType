#pragma once

#include <string>

#include "engine/core/NonCopyableNonMovable.hpp"
#include "engine/core/Vec2.hpp"
#include "engine/graphics/Export.hpp"
#include "engine/graphics/Types.hpp"

namespace engine::graphics {

/**
 * @brief Window and frame management provided by a graphics backend.
 *
 * Coordinates are virtual units (see virtual_size()); the backend scales them to the window.
 *
 * @note Every member function must be called from the main thread, the one that called open().
 */
class ENGINE_GRAPHICS_EXPORT IRenderer : core::NonCopyableNonMovable {
public:
    virtual ~IRenderer();

    /**
     * @brief Opens the window.
     *
     * @param config Size, title and fullscreen mode.
     * @return false if the window could not be created; the renderer stays closed.
     */
    virtual bool open(const WindowConfig& config) = 0;

    /** @brief Closes the window. Harmless if it is not open. */
    virtual void close() = 0;

    /** @return true once the user asked to close the window, or if it is not open. */
    [[nodiscard]] virtual bool should_close() const = 0;

    /** @param enabled true for fullscreen, false for windowed. */
    virtual void set_fullscreen(bool enabled) = 0;

    /** @return The size of the drawing space in virtual units, whatever the window size. */
    [[nodiscard]] virtual core::Vec2 virtual_size() const = 0;

    /**
     * @brief Loads an image file into a texture.
     *
     * @param path Image file, absolute or relative to the working directory.
     * @return A new texture, or the placeholder (default TextureId) if the file cannot be loaded
     * or the window is not open. The failure is logged.
     */
    [[nodiscard]] virtual TextureId load_texture(const std::string& path) = 0;

    /**
     * @brief Frees a texture returned by load_texture().
     *
     * @note Harmless on the placeholder or on an unknown texture. Every texture is freed by
     * close().
     */
    virtual void unload_texture(TextureId texture) = 0;

    /** @return The size of the texture in pixels; the placeholder's size for an unknown one. */
    [[nodiscard]] virtual core::Vec2 texture_size(TextureId texture) const = 0;

    /**
     * @brief Starts a frame and clears it.
     *
     * @param clear Color of the whole window, letterbox bars included.
     */
    virtual void begin_frame(Color clear) = 0;

    /** @brief Presents the frame started by begin_frame(). */
    virtual void end_frame() = 0;

    /**
     * @brief Draws part of a texture.
     *
     * @note An unknown texture is drawn as the placeholder. Like every draw call, it only has an
     * effect between begin_frame() and end_frame().
     */
    virtual void draw_sprite(const SpriteDraw& sprite) = 0;

    /** @param filled false draws only the outline, one virtual unit thick. */
    virtual void draw_rect(const Rect& rect, Color color, bool filled = true) = 0;

    /** @brief Draws a filled circle. */
    virtual void draw_circle(core::Vec2 center, float radius, Color color) = 0;

    /** @param thickness Width of the line in virtual units. */
    virtual void draw_line(core::Vec2 from, core::Vec2 to, float thickness, Color color) = 0;

    /**
     * @brief Draws one line of text with the default font.
     *
     * @param position Top-left corner of the text.
     * @param size Height of the text in virtual units.
     */
    virtual void draw_text(const std::string& text, core::Vec2 position, float size,
                           Color color) = 0;

    /** @return The size draw_text() would cover with the same text and size. */
    [[nodiscard]] virtual core::Vec2 measure_text(const std::string& text, float size) const = 0;
};

} // namespace engine::graphics
