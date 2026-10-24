local config = require("config")
local roster = require("roster")

entity "missile"

tags   = { "missile" }
sprite = "sprites/missile.png"
speed  = config.missile.speed
hitbox = config.missile.hitbox

function on_spawn(self)
    roster.add(self, "missiles")
end

function on_destroy(self)
    roster.remove(self)
end

function on_hit(self, other)
    if other:has_tag("enemy") then
        roster.destroy(self)
    end
end

function update(self, dt)
    self.x = self.x + self.speed * dt
    if self.x >= config.world.width then
        roster.destroy(self)
    end
end
