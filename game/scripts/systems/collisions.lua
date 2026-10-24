local aabb = require("aabb")
local roster = require("roster")

entity "collisions"

local function collide(group, other_group)
    for _, first in ipairs(roster.list(group)) do
        for _, second in ipairs(roster.list(other_group)) do
            if roster.contains(first) and roster.contains(second) and aabb.overlaps(first, second) then
                first:on_hit(second)
                second:on_hit(first)
            end
        end
    end
end

function update(self, dt)
    collide("missiles", "enemies")
    collide("players", "enemies")
end
