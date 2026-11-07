#include <gtest/gtest.h>

#include <algorithm>
#include <map>
#include <optional>
#include <string>
#include <string_view>

#include "../../engine/script/scene/TempFolder.hpp"
#include "settings/Settings.hpp"

using editor::LoadedSettings;
using editor::parse_settings;
using editor::Settings;
using editor::Severity;

namespace {

LoadedSettings parsed(std::string_view text) { return parse_settings(text, "settings.json"); }

bool reports(const LoadedSettings& loaded, Severity severity, std::string_view part) {
    return std::ranges::any_of(loaded.problems, [&](const editor::ConsoleLine& line) {
        return line.severity == severity && line.text.find(part) != std::string::npos;
    });
}

editor::Environment environment(std::map<std::string, std::string, std::less<>> values) {
    return [values = std::move(values)](std::string_view name) -> std::optional<std::string> {
        const auto found = values.find(name);
        return found == values.end() ? std::nullopt : std::optional<std::string>(found->second);
    };
}

} // namespace

TEST(Settings, DefaultsMatchTheEditor) {
    const Settings defaults;

    EXPECT_EQ(defaults.ui_scale, 1.F);
    EXPECT_EQ(defaults.scroll_speed, 1.F);
    EXPECT_EQ(defaults.view, editor::ViewSettings{});
}

TEST(Settings, EmptyObjectKeepsTheDefaults) {
    const LoadedSettings loaded = parsed("{}");

    EXPECT_EQ(loaded.settings, Settings{});
    EXPECT_TRUE(loaded.problems.empty());
}

TEST(Settings, ValidFileSetsEveryKey) {
    const LoadedSettings loaded = parsed(R"({
        "ui.scale": 1.5,
        "ui.scroll_speed": 2,
        "view.snap": false,
        "view.grid_size": 32,
        "view.show_hitboxes": true,
        "view.show_names": false
    })");

    EXPECT_TRUE(loaded.problems.empty());
    EXPECT_EQ(loaded.settings, (Settings{.ui_scale = 1.5F,
                                         .scroll_speed = 2.F,
                                         .view = {.snap = false,
                                                  .grid_size = 32.F,
                                                  .show_hitboxes = true,
                                                  .show_names = false}}));
}

TEST(Settings, InvalidJsonGivesTheDefaultsAndOneError) {
    const LoadedSettings loaded = parsed(R"({"ui.scale": 2,)");

    EXPECT_EQ(loaded.settings, Settings{});
    ASSERT_EQ(loaded.problems.size(), 1U);
    EXPECT_TRUE(reports(loaded, Severity::error, "settings.json: invalid JSON"));
}

TEST(Settings, NonObjectGivesTheDefaultsAndOneError) {
    const LoadedSettings loaded = parsed(R"([1, 2])");

    EXPECT_EQ(loaded.settings, Settings{});
    ASSERT_EQ(loaded.problems.size(), 1U);
    EXPECT_TRUE(reports(loaded, Severity::error, "must hold a JSON object"));
}

TEST(Settings, UnknownKeyIsWarnedAndTheOthersApply) {
    const LoadedSettings loaded = parsed(R"({"ui.font_size": 18, "view.snap": false})");

    EXPECT_TRUE(reports(loaded, Severity::warning, "unknown key 'ui.font_size'"));
    EXPECT_FALSE(loaded.settings.view.snap);
}

TEST(Settings, WrongTypesKeepTheirDefault) {
    const LoadedSettings loaded =
        parsed(R"({"ui.scale": "big", "view.snap": "yes", "view.grid_size": true})");

    EXPECT_EQ(loaded.settings, Settings{});
    EXPECT_EQ(loaded.problems.size(), 3U);
    EXPECT_TRUE(reports(loaded, Severity::warning, "'ui.scale' must be a number from 0.5 to 3"));
    EXPECT_TRUE(reports(loaded, Severity::warning, "'view.snap' must be true or false"));
    EXPECT_TRUE(reports(loaded, Severity::warning, "'view.grid_size' must be a number"));
}

TEST(Settings, OutOfRangeValuesKeepTheirDefault) {
    const LoadedSettings loaded =
        parsed(R"({"ui.scale": 10, "ui.scroll_speed": 0.1, "view.grid_size": 0})");

    EXPECT_EQ(loaded.settings, Settings{});
    EXPECT_EQ(loaded.problems.size(), 3U);
    EXPECT_TRUE(reports(loaded, Severity::warning, "kept 1"));
    EXPECT_TRUE(reports(loaded, Severity::warning,
                        "'view.grid_size' must be a number from 1 to "
                        "512; kept 16"));
}

TEST(Settings, RangesIncludeTheirEnds) {
    const LoadedSettings loaded =
        parsed(R"({"ui.scale": 3, "ui.scroll_speed": 0.25, "view.grid_size": 512})");

    EXPECT_TRUE(loaded.problems.empty());
    EXPECT_EQ(loaded.settings.ui_scale, 3.F);
    EXPECT_EQ(loaded.settings.scroll_speed, 0.25F);
    EXPECT_EQ(loaded.settings.view.grid_size, 512.F);
}

TEST(Settings, MissingFileGivesTheDefaultsQuietly) {
    TempFolder folder;
    const LoadedSettings loaded = editor::load_settings(folder.path() / "settings.json");

    EXPECT_EQ(loaded.settings, Settings{});
    EXPECT_TRUE(loaded.problems.empty());
}

TEST(Settings, LoadReadsTheFileAndNamesItInProblems) {
    TempFolder folder;
    folder.write("settings.json", R"({"view.show_names": false, "nope": 1})");

    const LoadedSettings loaded = editor::load_settings(folder.path() / "settings.json");

    EXPECT_FALSE(loaded.settings.view.show_names);
    EXPECT_TRUE(reports(loaded, Severity::warning,
                        (folder.path() / "settings.json").generic_string() + ": unknown key"));
}

TEST(Settings, LoadNeverWritesTheFile) {
    TempFolder folder;
    static_cast<void>(editor::load_settings(folder.path() / "settings.json"));

    EXPECT_FALSE(std::filesystem::exists(folder.path() / "settings.json"));
}

#if defined(__APPLE__)
TEST(SettingsPath, LivesInApplicationSupport) {
    EXPECT_EQ(
        editor::settings_path(environment({{"HOME", "/Users/me"}})),
        std::filesystem::path("/Users/me/Library/Application Support/engine-editor/settings.json"));
}
#else
TEST(SettingsPath, UsesTheXdgConfigFolder) {
    EXPECT_EQ(
        editor::settings_path(environment({{"XDG_CONFIG_HOME", "/cfg"}, {"HOME", "/home/me"}})),
        std::filesystem::path("/cfg/engine-editor/settings.json"));
}

TEST(SettingsPath, FallsBackToDotConfig) {
    EXPECT_EQ(editor::settings_path(environment({{"HOME", "/home/me"}})),
              std::filesystem::path("/home/me/.config/engine-editor/settings.json"));
    EXPECT_EQ(editor::settings_path(environment({{"XDG_CONFIG_HOME", ""}, {"HOME", "/home/me"}})),
              std::filesystem::path("/home/me/.config/engine-editor/settings.json"));
}
#endif

TEST(SettingsPath, NothingWithoutAHome) {
    EXPECT_EQ(editor::settings_path(environment({})), std::nullopt);
}

TEST(Settings, LoadRemembersTheFile) {
    TempFolder folder;

    EXPECT_EQ(editor::load_settings(folder.path() / "settings.json").file,
              folder.path() / "settings.json");
}

TEST(SettingsFile, CreatesAnEmptyObjectAndItsFolder) {
    TempFolder folder;
    const auto file = folder.path() / "engine-editor" / "settings.json";

    const auto result = editor::create_settings_file(file);

    ASSERT_TRUE(result.ok) << result.message;
    EXPECT_EQ(folder.read("engine-editor/settings.json"), "{}\n");
    EXPECT_TRUE(editor::load_settings(file).problems.empty());
}

TEST(SettingsFile, NeverChangesAnExistingFile) {
    TempFolder folder;
    folder.write("settings.json", R"({"ui.scale": 2})");

    ASSERT_TRUE(editor::create_settings_file(folder.path() / "settings.json").ok);

    EXPECT_EQ(folder.read("settings.json"), R"({"ui.scale": 2})");
}

TEST(SettingsFile, ReportsAFolderThatCannotBeMade) {
    TempFolder folder;
    folder.write("blocked", "a file, not a folder");

    const auto result = editor::create_settings_file(folder.path() / "blocked" / "settings.json");

    EXPECT_FALSE(result.ok);
    EXPECT_NE(result.message.find("cannot create"), std::string::npos);
}
