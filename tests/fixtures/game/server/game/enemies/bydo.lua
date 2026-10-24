local config = require("config")
local roster = require("roster")

entity "bydo"

tags          = { "enemy" }
sprite        = "sprites/bydo.png"
speed         = config.bydo.speed
hitbox        = config.bydo.hitbox
collides_with = { "missile" }

function on_spawn(self)
    roster.add(self, "enemies")
end

function on_destroy(self)
    roster.remove(self)
end

function on_collide(self, other)
    play("explosion.wav", self)
    roster.destroy(self)
end

function update(self, dt)
    self.x = self.x - self.speed * dt
    if self.x <= -self.hitbox.w then
        roster.destroy(self)
    end
end
