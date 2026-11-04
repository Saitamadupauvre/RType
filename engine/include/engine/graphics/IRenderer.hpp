#pragma once

#include <string>

#include "engine/core/math/Camera.hpp"
#include "engine/core/math/Vec2.hpp"
#include "engine/core/util/NonCopyableNonMovable.hpp"
#include "engine/graphics/Export.hpp"
#include "engine/graphics/Types.hpp"

namespace engine::graphics {

/**
 * @brief Window and frame management provided by a graphics backend.
 *
 * Coordinates are virtual units (see virtual_size()); the backend scales them to the window.
 * Draw calls are in screen space, unless they happen between begin_world() and end_world(): they
 * are then in world units, seen through a camera view.
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

    /**
     * @brief Closes the window and frees every texture, render target and shader.
     *
     * @note Harmless if it is not open.
     */
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
     * @brief Creates an off-screen surface that draw calls can be redirected to.
     *
     * @param size Size of the surface in pixels; both components must be at least 1. Backends
     * may refuse sizes their GPU cannot hold (16384 for graphics-raylib).
     * @return A new target, or the default RenderTargetId if the size is invalid, the window is
     * not open or the backend fails. The failure is logged.
     */
    [[nodiscard]] virtual RenderTargetId create_render_target(core::Vec2 size) = 0;

    /**
     * @brief Frees a target returned by create_render_target().
     *
     * @note Ends drawing into it first if it is the current target. Its texture becomes unknown.
     * Harmless on the default or an unknown target. Every target is freed by close().
     */
    virtual void destroy_render_target(RenderTargetId target) = 0;

    /**
     * @brief Redirects the following draw calls into @p target and clears it.
     *
     * Inside the target, screen space is its own size in pixels, and begin_world() centers the
     * view on it.
     *
     * @param target Target to draw into.
     * @param clear Color the whole target is cleared to.
     * @note Ends world-space drawing first. Calling it again before end_target() switches to the
     * new target. Does nothing outside begin_frame() / end_frame() or for an unknown target.
     */
    virtual void begin_target(RenderTargetId target, Color clear) = 0;

    /**
     * @brief Sends the following draw calls back to the frame.
     *
     * @note Ends world-space drawing first. Harmless when no target is being drawn into.
     */
    virtual void end_target() = 0;

    /**
     * @return The texture holding what was drawn into @p target, to draw with draw_sprite() like
     * any texture, upright; the default TextureId for an unknown target.
     * @note The texture belongs to the target: unload_texture() ignores it.
     */
    [[nodiscard]] virtual TextureId target_texture(RenderTargetId target) const = 0;

    /**
     * @brief Compiles a shader program from GLSL source files.
     *
     * @param vertex Vertex shader file; empty uses the backend's default vertex shader.
     * @param fragment Fragment shader file; empty uses the backend's default fragment shader.
     * @return A new shader, or the default ShaderId if a file is missing, the program does not
     * compile or link, or the window is not open. The failure is logged.
     */
    [[nodiscard]] virtual ShaderId load_shader(const std::string& vertex,
                                               const std::string& fragment) = 0;

    /**
     * @brief Frees a shader returned by load_shader().
     *
     * @note Ends it first if it is the current shader. Harmless on the default or an unknown
     * shader. Every shader is freed by close().
     */
    virtual void unload_shader(ShaderId shader) = 0;

    /**
     * @brief Draws the following calls through @p shader until end_shader().
     *
     * @note Calling it again replaces the current shader. The default or an unknown shader goes
     * back to the default pipeline, so drawing always continues. Does nothing outside
     * begin_frame() / end_frame().
     */
    virtual void begin_shader(ShaderId shader) = 0;

    /** @brief Goes back to the default pipeline. Harmless when no shader is in use. */
    virtual void end_shader() = 0;

    /**
     * @brief Sets a `float` uniform of @p shader.
     *
     * @note Values are kept by the shader across frames. Affects only the draw calls made
     * after it. An unknown shader or a uniform the program does not use is ignored.
     */
    virtual void set_uniform(ShaderId shader, const std::string& name, float value) = 0;

    /** @brief Sets a `vec2` uniform of @p shader; same rules as the `float` overload. */
    virtual void set_uniform(ShaderId shader, const std::string& name, core::Vec2 value) = 0;

    /**
     * @brief Sets a `vec4` uniform of @p shader to @p value with each channel in [0, 1]; same
     * rules as the `float` overload.
     */
    virtual void set_uniform(ShaderId shader, const std::string& name, Color value) = 0;

    /**
     * @brief Binds @p value to a `sampler2D` uniform of @p shader; same rules as the `float`
     * overload.
     *
     * @note An unknown texture binds the placeholder.
     */
    virtual void set_uniform(ShaderId shader, const std::string& name, TextureId value) = 0;

    /**
     * @brief Starts a frame and clears it.
     *
     * @param clear Color of the whole window, letterbox bars included.
     */
    virtual void begin_frame(Color clear) = 0;

    /**
     * @brief Presents the frame started by begin_frame().
     *
     * @note Ends world-space drawing, the current target and the current shader first if they
     * were not ended.
     */
    virtual void end_frame() = 0;

    /**
     * @brief Starts world-space drawing: until end_world(), draw calls take world units and are
     * transformed by @p view, see world_to_screen().
     *
     * @param view Camera view; its zoom must be strictly positive.
     * @note Calling it again before end_world() replaces the view. Only has an effect between
     * begin_frame() and end_frame().
     */
    virtual void begin_world(const core::CameraView& view) = 0;

    /** @brief Goes back to screen-space drawing. Harmless outside world-space drawing. */
    virtual void end_world() = 0;

    /**
     * @return Where the world point @p point is drawn on screen, in virtual units, when drawing
     * through @p view.
     */
    [[nodiscard]] core::Vec2 world_to_screen(const core::CameraView& view, core::Vec2 point) const;

    /** @return The world point drawn at the screen point @p point through @p view. */
    [[nodiscard]] core::Vec2 screen_to_world(const core::CameraView& view, core::Vec2 point) const;

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
