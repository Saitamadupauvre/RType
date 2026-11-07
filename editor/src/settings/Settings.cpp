#include "settings/Settings.hpp"

#include <array>
#include <cmath>
#include <format>
#include <fstream>
#include <iterator>
#include <system_error>
#include <utility>

#include <nlohmann/json.hpp>

namespace editor {

namespace {

using Json = nlohmann::json;

constexpr std::string_view folder_name = "engine-editor";
constexpr std::string_view file_name = "settings.json";

struct NumberKey {
    std::string_view name;
    float min;
    float max;
    float Settings::* field;
};

struct FlagKey {
    std::string_view name;
    bool ViewSettings::* field;
};

constexpr std::array number_keys{
    NumberKey{.name = "ui.scale", .min = 0.5F, .max = 3.F, .field = &Settings::ui_scale},
    NumberKey{
        .name = "ui.scroll_speed", .min = 0.25F, .max = 4.F, .field = &Settings::scroll_speed},
};

constexpr float min_grid = 1.F;
constexpr float max_grid = 512.F;

constexpr std::array flag_keys{
    FlagKey{.name = "view.snap", .field = &ViewSettings::snap},
    FlagKey{.name = "view.show_hitboxes", .field = &ViewSettings::show_hitboxes},
    FlagKey{.name = "view.show_names", .field = &ViewSettings::show_names},
};

class Reader {
public:
    explicit Reader(std::string_view source) : _source(source) {}

    void read(const std::string& key, const Json& value) {
        if (key == "view.grid_size") {
            read_number(key, value, min_grid, max_grid, _loaded.settings.view.grid_size);
            return;
        }
        for (const NumberKey& number : number_keys) {
            if (key == number.name) {
                read_number(key, value, number.min, number.max, _loaded.settings.*number.field);
                return;
            }
        }
        for (const FlagKey& flag : flag_keys) {
            if (key == flag.name) {
                read_flag(key, value, _loaded.settings.view.*flag.field);
                return;
            }
        }
        warn(std::format("unknown key '{}' ignored", key));
    }

    void fail(const std::string& reason) {
        _loaded = LoadedSettings{};
        _loaded.problems.push_back(
            {.severity = Severity::error,
             .text = std::format("{}: {}; using the default settings", _source, reason)});
    }

    LoadedSettings take() { return std::move(_loaded); }

private:
    void read_number(const std::string& key, const Json& value, float min, float max,
                     float& field) {
        const bool number = value.is_number() && std::isfinite(value.get<double>());
        const auto read = number ? value.get<float>() : 0.F;
        if (!number || read < min || read > max) {
            warn(std::format("'{}' must be a number from {:g} to {:g}; kept {:g}", key, min, max,
                             field));
            return;
        }
        field = read;
    }

    void read_flag(const std::string& key, const Json& value, bool& field) {
        if (!value.is_boolean()) {
            warn(std::format("'{}' must be true or false; kept {}", key, field));
            return;
        }
        field = value.get<bool>();
    }

    void warn(const std::string& text) {
        _loaded.problems.push_back(
            {.severity = Severity::warning, .text = std::format("{}: {}", _source, text)});
    }

    std::string_view _source;
    LoadedSettings _loaded;
};

} // namespace

std::optional<std::filesystem::path> settings_path(const Environment& environment) {
    const std::optional<std::string> home = environment("HOME");
#if defined(__APPLE__)
    if (!home || home->empty()) {
        return std::nullopt;
    }
    return std::filesystem::path(*home) / "Library" / "Application Support" / folder_name /
           file_name;
#else
    if (const std::optional<std::string> config = environment("XDG_CONFIG_HOME");
        config && !config->empty()) {
        return std::filesystem::path(*config) / folder_name / file_name;
    }
    if (!home || home->empty()) {
        return std::nullopt;
    }
    return std::filesystem::path(*home) / ".config" / folder_name / file_name;
#endif
}

LoadedSettings parse_settings(std::string_view text, std::string_view source) {
    Reader reader(source);
    const Json json = Json::parse(text, nullptr, false);
    if (json.is_discarded()) {
        reader.fail("invalid JSON");
    } else if (!json.is_object()) {
        reader.fail("must hold a JSON object");
    } else {
        for (const auto& [key, value] : json.items()) {
            reader.read(key, value);
        }
    }
    return reader.take();
}

LoadedSettings load_settings(const std::filesystem::path& file) {
    std::error_code error;
    if (!std::filesystem::exists(file, error)) {
        return {};
    }
    std::ifstream stream(file, std::ios::binary);
    const std::string text{std::istreambuf_iterator<char>(stream), {}};
    if (!stream && !stream.eof()) {
        LoadedSettings unreadable;
        unreadable.problems.push_back(
            {.severity = Severity::error,
             .text = std::format("{}: cannot be read; using the default settings",
                                 file.generic_string())});
        return unreadable;
    }
    return parse_settings(text, file.generic_string());
}

} // namespace editor
