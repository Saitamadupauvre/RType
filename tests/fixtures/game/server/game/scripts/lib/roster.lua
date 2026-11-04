local roster = { groups = {} }

function roster.add(entity, group)
    roster.groups[group] = roster.groups[group] or {}
    roster.groups[group][entity.id] = entity
end

function roster.remove(entity)
    for _, members in pairs(roster.groups) do
        members[entity.id] = nil
    end
end

function roster.contains(entity)
    for _, members in pairs(roster.groups) do
        if members[entity.id] then
            return true
        end
    end
    return false
end

function roster.list(group)
    local members = {}
    for _, entity in pairs(roster.groups[group] or {}) do
        members[#members + 1] = entity
    end
    table.sort(members, function(a, b) return a.id < b.id end)
    return members
end

function roster.destroy(entity)
    roster.remove(entity)
    entity:destroy()
end

return roster
