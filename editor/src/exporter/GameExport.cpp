#include "exporter/GameExport.hpp"

#include <algorithm>
#include <cctype>
#include <format>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <system_error>
#include <utility>

#include "engine/script/GameFolder.hpp"

namespace editor {

namespace {

namespace fs = std::filesystem;

constexpr std::string_view runtime_binary = "engine-runtime";
constexpr std::string_view library_prefix = "libengine-";
constexpr std::string_view fallback_name = "game";
constexpr std::string_view readme_file = "README.txt";

fs::path folder_of(const fs::path& path) {
    fs::path folder = fs::absolute(path).lexically_normal();
    return folder.has_filename() ? folder : folder.parent_path();
}

bool inside(const fs::path& path, const fs::path& folder) {
    const fs::path relative = folder_of(path).lexically_relative(folder_of(folder));
    return !relative.empty() && *relative.begin() != "..";
}

bool present(const fs::path& path) {
    std::error_code error;
    return fs::exists(path, error);
}

bool empty_folder(const fs::path& path) {
    std::error_code error;
    return fs::is_directory(path, error) && fs::is_empty(path, error);
}

void add_binary(ExportPlan& plan, const fs::path& runtime, std::string_view binary,
                const std::string& name) {
    const fs::path from = runtime / binary;
    if (!present(from)) {
        plan.problems.push_back(std::format("runtime not found: {}", from.generic_string()));
        return;
    }
    plan.steps.push_back({.from = from, .to = plan.output / name});
}

void add_libraries(ExportPlan& plan, const fs::path& runtime) {
    std::error_code error;
    std::vector<fs::path> libraries;
    for (const fs::directory_entry& entry : fs::directory_iterator(runtime, error)) {
        const std::string file = entry.path().filename().string();
        if (file.starts_with(library_prefix) && entry.is_regular_file(error)) {
            libraries.push_back(entry.path());
        }
    }
    std::ranges::sort(libraries);
    for (const fs::path& library : libraries) {
        plan.steps.push_back({.from = library, .to = plan.output / library.filename()});
    }
}

void add_entry(ExportPlan& plan, const fs::path& from, bool required) {
    if (!present(from)) {
        if (required) {
            plan.problems.push_back(std::format("missing: {}", from.generic_string()));
        }
        return;
    }
    plan.steps.push_back({.from = from, .to = plan.output / from.filename()});
}

void check_output(ExportPlan& plan, const fs::path& game, const ExportSettings& settings) {
    if (settings.output.empty()) {
        plan.problems.emplace_back("choose an output folder");
        return;
    }
    if (folder_of(settings.output) == folder_of(game) || inside(settings.output, game)) {
        plan.problems.emplace_back("the output folder cannot be inside the game folder");
    } else if (present(settings.output) && !empty_folder(settings.output)) {
        plan.problems.push_back(
            std::format("{} already exists and is not empty", settings.output.generic_string()));
    }
}

std::string readme(const ExportSettings& settings) {
    const ExportModes& modes = settings.modes;
    std::string text = std::format("{}\n\n", settings.name);
    if (modes.solo) {
        text += std::format("Play alone:             ./{} --solo\n", settings.name);
    }
    if (modes.host) {
        text += std::format("Host and play:          ./{} --host 4242\n", settings.name);
    }
    if (modes.client) {
        text += std::format("Join a game:            ./{} <server-ip> 4242\n", settings.name);
    }
    if (modes.dedicated) {
        text += std::format("Dedicated server:       ./{}-server 4242\n", settings.name);
    }
    return text;
}

std::optional<ExportLauncher> server_launcher(const ExportSettings& settings) {
    if (!settings.modes.dedicated) {
        return std::nullopt;
    }
    return ExportLauncher{
        .to = settings.output / (settings.name + "-server"),
        .text = std::format("#!/bin/sh\nexec \"$(dirname \"$0\")/{}\" --server \"$@\"\n",
                            settings.name)};
}

ExportModes modes_of(const std::optional<std::vector<std::string>>& names) {
    if (!names) {
        return {};
    }
    const auto offers = [&](std::string_view mode) {
        return std::ranges::find(*names, mode) != names->end();
    };
    return {.solo = offers("solo"),
            .host = offers("host"),
            .client = offers("client"),
            .dedicated = offers("dedicated")};
}

void write_launcher(const ExportLauncher& launcher) {
    std::ofstream out(launcher.to);
    out << launcher.text;
    out.close();
    if (!out) {
        throw std::runtime_error(std::format("cannot write {}", launcher.to.filename().string()));
    }
    fs::permissions(launcher.to,
                    fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec,
                    fs::perm_options::add);
}

} // namespace

std::vector<std::string> mode_names(const ExportModes& modes) {
    std::vector<std::string> names;
    for (const auto [offered, name] :
         {std::pair{modes.solo, "solo"}, std::pair{modes.host, "host"},
          std::pair{modes.client, "client"}, std::pair{modes.dedicated, "dedicated"}}) {
        if (offered) {
            names.emplace_back(name);
        }
    }
    return names;
}

std::string export_name(std::string_view game_name) {
    std::string name;
    for (const char character : game_name) {
        const auto byte = static_cast<unsigned char>(character);
        if (std::isalnum(byte) != 0 || character == '_' || character == '-') {
            name += static_cast<char>(std::tolower(byte));
        } else if (!name.empty() && name.back() != '-') {
            name += '-';
        }
    }
    while (!name.empty() && name.back() == '-') {
        name.pop_back();
    }
    return engine::script::is_scene_name(name) ? name : std::string(fallback_name);
}

ExportSettings default_export(const fs::path& game) {
    const fs::path folder = folder_of(game);
    const auto project = engine::script::read_project(folder);
    const std::string game_name =
        project && project->name ? *project->name : folder.filename().string();
    std::string name = export_name(game_name);
    fs::path output = folder.parent_path() / (name + "-export");
    return {.name = std::move(name),
            .output = std::move(output),
            .modes = modes_of(project ? project->modes : std::nullopt)};
}

ExportPlan plan_export(const fs::path& game, const fs::path& runtime,
                       const ExportSettings& settings) {
    ExportPlan plan{.output = settings.output,
                    .steps = {},
                    .problems = {},
                    .readme = readme(settings),
                    .modes = mode_names(settings.modes),
                    .server_launcher = server_launcher(settings)};
    if (!engine::script::is_scene_name(settings.name)) {
        plan.problems.emplace_back("the name takes 1 to 63 letters, digits, '_' or '-'");
    }
    if (plan.modes.empty()) {
        plan.problems.emplace_back("choose at least one mode");
    }
    check_output(plan, game, settings);
    add_binary(plan, runtime, runtime_binary, settings.name);
    add_libraries(plan, runtime);
    add_entry(plan, runtime / "plugins", true);
    add_entry(plan, game / "game.json", true);
    add_entry(plan, engine::script::scripts_folder(game), true);
    add_entry(plan, game / "assets", false);
    add_entry(plan, game / "scenes", false);
    return plan;
}

engine::editor::CommandResult run_export(const ExportPlan& plan) {
    try {
        fs::create_directories(plan.output);
        for (const ExportStep& step : plan.steps) {
            fs::copy(step.from, step.to, fs::copy_options::recursive);
        }
        if (!engine::script::write_modes(plan.output, plan.modes)) {
            return engine::editor::CommandResult::refused(
                "export failed: cannot write the modes into game.json");
        }
        if (plan.server_launcher) {
            write_launcher(*plan.server_launcher);
        }
        std::ofstream readme_out(plan.output / readme_file);
        readme_out << plan.readme;
        if (!readme_out) {
            return engine::editor::CommandResult::refused(
                std::format("export failed: cannot write {}", readme_file));
        }
    } catch (const std::exception& error) {
        return engine::editor::CommandResult::refused(
            std::format("export failed: {}", error.what()));
    }
    return engine::editor::CommandResult::done(
        std::format("exported to {} ({} items)", plan.output.generic_string(), plan.steps.size()));
}

} // namespace editor
