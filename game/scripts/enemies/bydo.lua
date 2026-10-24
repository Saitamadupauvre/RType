local config = require("config")
local roster = require("roster")

entity "bydo"

tags          = { "enemy" }
sprite        = "sprites/bydo.png"
speed         = config.bydo.speed
hitbox        = config.bydo.hitbox
collides_with = { "missile" }

function on_spawn(self)
    self.reload = config.bydo.first_shot_delay
    roster.add(self, "enemies")
end

function on_destroy(self)
    roster.remove(self)
end

function on_collide(self, other)
    play("explosion.wav", self)
    roster.destroy(self)
end

function fire(self)
    local shot = config.bydo_shot.hitbox
    spawn("bydo_shot", {
        x = self.x - shot.w,
        y = self.y + (self.hitbox.h - shot.h) / 2,
    })
    play("laser_shoot.wav", self)
end

function update(self, dt)
    self.x = self.x - self.speed * dt
    if self.x <= -self.hitbox.w then
        roster.destroy(self)
        return
    end
    self.reload = self.reload - dt
    if self.reload <= 0 then
        self.reload = self.reload + config.bydo.fire_interval
        self:fire()
    end
end
