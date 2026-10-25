local config = require("config")
local roster = require("roster")

entity "bydo_shot"

tags          = { "enemy_shot" }
sprite        = "sprites/bydo_shot.png"
speed         = config.bydo_shot.speed
hitbox        = config.bydo_shot.hitbox
collides_with = { "player" }

function on_spawn(self)
    roster.add(self, "enemy_shots")
end

function on_destroy(self)
    roster.remove(self)
end

function on_collide(self, other)
    roster.destroy(self)
end

function update(self, dt)
    self.x = self.x - self.speed * dt
    if self.x <= -self.hitbox.w then
        roster.destroy(self)
    end
end
