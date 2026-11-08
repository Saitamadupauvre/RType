#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

#include "../../engine/script/scene/TempFolder.hpp"
#include "exporter/GameExport.hpp"

using editor::Binaries;
using editor::ExportPlan;
using editor::ExportSettings;
using editor::ExportStep;

namespace fs = std::filesystem;

namespace {

class GameExport : public ::testing::Test {
protected:
    TempFolder _root;
    fs::path _game = _root.path() / "game";
    fs::path _runtime = _root.path() / "runtime";
    fs::path _output = _root.path() / "out";

    GameExport() {
        _root.write("game/game.json", R"({"name": "Space Game", "start_scene": "level1"})");
        _root.write("game/scripts/main.lua", "");
        _root.write("game/assets/ship.png", "png");
        _root.write("game/scenes/level1.json", "{}");
        _root.write("runtime/r-type_client", "client");
        _root.write("runtime/r-type_server", "server");
        _root.write("runtime/libengine-core.so", "core");
        _root.write("runtime/libengine-app.so", "app");
        _root.write("runtime/libother.so", "other");
        _root.write("runtime/editor", "editor");
        _root.write("runtime/plugins/graphics-raylib.so", "graphics");
        fs::permissions(_runtime / "r-type_client", fs::perms::owner_exec, fs::perm_options::add);
    }

    [[nodiscard]] ExportSettings settings(Binaries binaries = Binaries::client_and_server) const {
        return {.name = "space", .output = _output, .binaries = binaries};
    }

    [[nodiscard]] ExportPlan plan(const ExportSettings& chosen) const {
        return editor::plan_export(_game, _runtime, chosen);
    }

    [[nodiscard]] static std::vector<std::string> targets(const ExportPlan& plan) {
        std::vector<std::string> names;
        names.reserve(plan.steps.size());
        for (const ExportStep& step : plan.steps) {
            names.push_back(step.to.filename().string());
        }
        return names;
    }

    [[nodiscard]] static bool mentions(const ExportPlan& plan, std::string_view text) {
        return std::ranges::any_of(plan.problems, [&](const std::string& problem) {
            return problem.find(text) != std::string::npos;
        });
    }
};

} // namespace

TEST(GameExportName, GameNamesBecomeBinaryNames) {
    EXPECT_EQ(editor::export_name("Space Game"), "space-game");
    EXPECT_EQ(editor::export_name("r-type"), "r-type");
    EXPECT_EQ(editor::export_name("  Hello,  World!  "), "hello-world");
    EXPECT_EQ(editor::export_name("my_game2"), "my_game2");
}

TEST(GameExportName, NamesWithoutUsableCharactersFallBack) {
    EXPECT_EQ(editor::export_name(""), "game");
    EXPECT_EQ(editor::export_name("!!!"), "game");
}

TEST_F(GameExport, DefaultsComeFromTheProject) {
    const ExportSettings defaults = editor::default_export(_game);

    EXPECT_EQ(defaults.name, "space-game");
    EXPECT_EQ(defaults.output, fs::absolute(_root.path() / "space-game-export").lexically_normal());
    EXPECT_EQ(defaults.binaries, Binaries::client_and_server);
}

TEST_F(GameExport, DefaultsUseTheFolderNameWithoutAProjectName) {
    _root.write("game/game.json", "{}");

    EXPECT_EQ(editor::default_export(_game / "").name, "game");
}

TEST_F(GameExport, PlanCopiesTheRuntimeAndTheGame) {
    const ExportPlan planned = plan(settings());

    EXPECT_TRUE(planned.problems.empty());
    EXPECT_EQ(
        targets(planned),
        (std::vector<std::string>{"space", "space-server", "libengine-app.so", "libengine-core.so",
                                  "plugins", "game.json", "scripts", "assets", "scenes"}));
    EXPECT_EQ(planned.steps.front(),
              (ExportStep{.from = _runtime / "r-type_client", .to = _output / "space"}));
}

TEST_F(GameExport, ClientOnlyLeavesTheServerOut) {
    const ExportPlan planned = plan(settings(Binaries::client_only));

    EXPECT_EQ(std::ranges::count(targets(planned), "space-server"), 0);
    EXPECT_EQ(planned.readme.find("space-server"), std::string::npos);
}

TEST_F(GameExport, ReadmeTellsHowToRunEveryMode) {
    const std::string readme = plan(settings()).readme;

    for (const std::string_view mode :
         {"./space --solo", "./space --host", "./space <server-ip>", "./space-server"}) {
        EXPECT_NE(readme.find(mode), std::string::npos) << mode;
    }
}

TEST_F(GameExport, OptionalGameFoldersMayBeMissing) {
    fs::remove_all(_game / "assets");
    fs::remove_all(_game / "scenes");

    const ExportPlan planned = plan(settings());

    EXPECT_TRUE(planned.problems.empty());
    EXPECT_EQ(std::ranges::count(targets(planned), "assets"), 0);
}

TEST_F(GameExport, BadNamesAreProblems) {
    for (const std::string& name :
         std::vector<std::string>{"", "../escape", "with space", std::string(64, 'a')}) {
        ExportSettings chosen = settings();
        chosen.name = name;
        EXPECT_TRUE(mentions(plan(chosen), "name")) << name;
    }
}

TEST_F(GameExport, MissingOutputIsAProblem) {
    ExportSettings chosen = settings();
    chosen.output.clear();

    EXPECT_TRUE(mentions(plan(chosen), "output folder"));
}

TEST_F(GameExport, OutputInsideTheGameIsAProblem) {
    for (const fs::path& output : {_game, _game / "build", _game / "scenes" / ".." / "out"}) {
        ExportSettings chosen = settings();
        chosen.output = output;
        EXPECT_TRUE(mentions(plan(chosen), "inside the game folder")) << output;
    }
}

TEST_F(GameExport, OutputNextToTheGameIsFine) {
    ExportSettings chosen = settings();
    chosen.output = _root.path() / "game-export";

    EXPECT_TRUE(plan(chosen).problems.empty());
}

TEST_F(GameExport, NonEmptyOutputIsAProblem) {
    _root.write("out/keep.txt", "mine");

    EXPECT_TRUE(mentions(plan(settings()), "not empty"));
}

TEST_F(GameExport, EmptyOutputIsFine) {
    fs::create_directories(_output);

    EXPECT_TRUE(plan(settings()).problems.empty());
}

TEST_F(GameExport, MissingRuntimePiecesAreProblems) {
    fs::remove(_runtime / "r-type_server");
    fs::remove_all(_runtime / "plugins");

    const ExportPlan planned = plan(settings());

    EXPECT_TRUE(mentions(planned, "r-type_server"));
    EXPECT_TRUE(mentions(planned, "plugins"));
    EXPECT_TRUE(plan(settings(Binaries::client_only)).problems.size() == 1);
}

TEST_F(GameExport, MissingScriptsAreAProblem) {
    fs::remove_all(_game / "scripts");

    EXPECT_TRUE(mentions(plan(settings()), "scripts"));
}

TEST_F(GameExport, RunWritesEveryFileAndTheReadme) {
    const ExportPlan planned = plan(settings());

    const auto result = editor::run_export(planned);

    ASSERT_TRUE(result.ok) << result.message;
    EXPECT_EQ(_root.read("out/space"), "client");
    EXPECT_EQ(_root.read("out/space-server"), "server");
    EXPECT_EQ(_root.read("out/libengine-core.so"), "core");
    EXPECT_EQ(_root.read("out/plugins/graphics-raylib.so"), "graphics");
    EXPECT_EQ(_root.read("out/scripts/main.lua"), "");
    EXPECT_EQ(_root.read("out/assets/ship.png"), "png");
    EXPECT_EQ(_root.read("out/README.txt"), planned.readme);
    EXPECT_FALSE(fs::exists(_output / "libother.so"));
    EXPECT_FALSE(fs::exists(_output / "editor"));
    EXPECT_NE(fs::status(_output / "space").permissions() & fs::perms::owner_exec, fs::perms::none);
}

TEST_F(GameExport, RunReportsACopyThatFails) {
    ExportPlan planned = plan(settings());
    planned.steps.push_back({.from = _runtime / "vanished", .to = _output / "vanished"});

    const auto result = editor::run_export(planned);

    EXPECT_FALSE(result.ok);
    EXPECT_NE(result.message.find("export failed"), std::string::npos);
}
