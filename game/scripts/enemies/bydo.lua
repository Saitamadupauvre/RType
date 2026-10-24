local config = require("config")
local roster = require("roster")

entity "bydo"

tags   = { "enemy" }
sprite = "sprites/bydo.png"
speed  = config.bydo.speed
hitbox = config.bydo.hitbox

function on_spawn(self)
    roster.add(self, "enemies")
end

function on_destroy(self)
    roster.remove(self)
end

function on_hit(self, other)
    if other:has_tag("missile") then
        play("explosion.wav", self)
        roster.destroy(self)
    end
end

function update(self, dt)
    self.x = self.x - self.speed * dt
    if self.x <= -self.hitbox.w then
        roster.destroy(self)
    end
end
