#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "Sol.hpp"
#include "engine/script/Declaration.hpp"

namespace engine::script {

struct Declaration {
    std::string name;
    DeclarationKind kind;
    std::filesystem::path source;
    sol::table prefab;
};

struct PendingDeclaration {
    std::string name;
    DeclarationKind kind;
    std::optional<sol::table> body;
};

struct PendingFile {
    std::filesystem::path source;
    std::vector<PendingDeclaration> declarations;
    bool open = true;
};

class DeclarationScope {
public:
    DeclarationScope(std::shared_ptr<PendingFile>& slot, std::shared_ptr<PendingFile> file);
    ~DeclarationScope();

    DeclarationScope(const DeclarationScope&) = delete;
    DeclarationScope& operator=(const DeclarationScope&) = delete;
    DeclarationScope(DeclarationScope&&) = delete;
    DeclarationScope& operator=(DeclarationScope&&) = delete;

private:
    std::shared_ptr<PendingFile>* _slot;
    std::shared_ptr<PendingFile> _previous;
};

std::string_view kind_name(DeclarationKind kind);

sol::object make_declarator(sol::this_state state, const std::shared_ptr<PendingFile>& file,
                            std::size_t index);

std::optional<std::vector<Declaration>> resolve_declarations(const PendingFile& file,
                                                             const sol::table& environment);

void warn_reserved_names(const Declaration& declaration);

std::string caller_location(lua_State* state);

} // namespace engine::script
