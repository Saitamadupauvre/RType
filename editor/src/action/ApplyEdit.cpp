#include "action/ApplyEdit.hpp"

#include <variant>

#include "action/Overloaded.hpp"

namespace editor {

using engine::editor::CommandResult;
using engine::editor::EditorSession;

CommandResult apply_edit(EditorSession& session, const Edit& edit) {
    return std::visit(
        Overloaded{
            [&](const edit::Place& place) { return session.place(place.type, place.position); },
            [&](const edit::Move& move) { return session.move(move.id, move.position); },
            [&](const edit::Remove& remove) { return session.remove(remove.id); },
            [&](const edit::Rename& rename) { return session.rename(rename.id, rename.name); },
            [&](const edit::SetField& set) {
                return session.set_field(set.id, set.key, set.value);
            },
            [&](const edit::ResetField& reset) { return session.reset_field(reset.id, reset.key); },
            [&](const edit::ResetAll& reset) { return session.reset_all(reset.id); },
            [&](const edit::ChangeType& change) {
                return session.change_type(change.id, change.type);
            },
            [&](const edit::Duplicate& duplicate) {
                return session.duplicate(duplicate.id, duplicate.offset);
            },
        },
        edit);
}

} // namespace editor
