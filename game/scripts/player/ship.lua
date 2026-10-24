local config = require("config")
local roster = require("roster")

entity "ship"

tags     = { "player", "ship" }
sprite   = "sprites/ship.png"
speed    = config.ship.speed
hitbox   = config.ship.hitbox
cooldown = 0

local function clamp(value, low, high)
    return math.max(low, math.min(value, high))
end

function on_spawn(self)
    roster.add(self, "players")
end

function on_destroy(self)
    roster.remove(self)
end

function on_hit(self, other)
    if other:has_tag("enemy") then
        play("explosion.wav", self)
        roster.destroy(self)
    end
end

function steer(self, input, dt)
    local bounds = config.world
    self.x = clamp(self.x + input:axis("move_x") * self.speed * dt, 0, bounds.width - self.hitbox.w)
    self.y = clamp(self.y + input:axis("move_y") * self.speed * dt, 0, bounds.height - self.hitbox.h)
end

function shoot(self, input, dt)
    self.cooldown = math.max(0, self.cooldown - dt)
    if not input:pressed("fire") or self.cooldown > 0 then
        return
    end
    spawn("missile", {
        x = self.x + self.hitbox.w,
        y = self.y + (self.hitbox.h - config.missile.hitbox.h) / 2,
    })
    play("laser_shoot.wav", self)
    self.cooldown = config.ship.fire_cooldown
end

function update(self, dt)
    local input = self.input
    self:steer(input, dt)
    self:shoot(input, dt)
end
