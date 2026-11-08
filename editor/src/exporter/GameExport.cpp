#include "exporter/GameExport.hpp"

#include <algorithm>
#include <cctype>
#include <format>
#include <fstream>
#include <system_error>

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
    return std::format("{0}\n\n"
                       "Play alone:             ./{0} --solo\n"
                       "Host and play:          ./{0} --host 4242\n"
                       "Join a game:            ./{0} <server-ip> 4242\n"
                       "Dedicated server:       ./{0} --server 4242\n",
                       settings.name);
}

} // namespace

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
    return {.name = std::move(name), .output = std::move(output)};
}

ExportPlan plan_export(const fs::path& game, const fs::path& runtime,
                       const ExportSettings& settings) {
    ExportPlan plan{
        .output = settings.output, .steps = {}, .problems = {}, .readme = readme(settings)};
    if (!engine::script::is_scene_name(settings.name)) {
        plan.problems.emplace_back("the name takes 1 to 63 letters, digits, '_' or '-'");
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
        std::ofstream readme_out(plan.output / readme_file);
        readme_out << plan.readme;
        if (!readme_out) {
            return engine::editor::CommandResult::refused(
                std::format("export failed: cannot write {}", readme_file));
        }
    } catch (const fs::filesystem_error& error) {
        return engine::editor::CommandResult::refused(
            std::format("export failed: {}", error.what()));
    }
    return engine::editor::CommandResult::done(
        std::format("exported to {} ({} items)", plan.output.generic_string(), plan.steps.size()));
}

} // namespace editor
