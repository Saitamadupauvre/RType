local config = require("config")
local roster = require("roster")

entity "bydo"

tags   = { "enemy" }
color  = { r = 230, g = 70, b = 70 }
speed  = config.bydo.speed
width  = config.bydo.width
height = config.bydo.height

function on_spawn(self)
    roster.add(self, "enemies")
end

function on_destroy(self)
    roster.remove(self)
end

function on_hit(self, other)
    if other:has_tag("missile") then
        roster.destroy(self)
    end
end

function update(self, dt)
    self.x = self.x - self.speed * dt
    if self.x <= -self.width then
        roster.destroy(self)
    end
end
