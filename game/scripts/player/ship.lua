local config = require("config")

entity "ship"

tags     = { "player" }
speed    = config.ship.speed
size     = config.ship.size
cooldown = 0

local function clamp(value, low, high)
    return math.max(low, math.min(value, high))
end

function steer(self, input, dt)
    local bounds = config.world
    self.x = clamp(self.x + input:axis("move_x") * self.speed * dt, 0, bounds.width - self.size)
    self.y = clamp(self.y + input:axis("move_y") * self.speed * dt, 0, bounds.height - self.size)
end

function shoot(self, input, dt)
    self.cooldown = math.max(0, self.cooldown - dt)
    if not input:pressed("fire") or self.cooldown > 0 then
        return
    end
    spawn("missile", {
        x = self.x + self.size,
        y = self.y + (self.size - config.missile.size) / 2,
    })
    self.cooldown = config.ship.fire_cooldown
end

function update(self, dt)
    local input = self.input
    self:steer(input, dt)
    self:shoot(input, dt)
end
