local config = require("config")

entity "ship"

tags  = { "player" }
speed = config.ship.speed
size  = config.ship.size

local function clamp(value, low, high)
    return math.max(low, math.min(value, high))
end

function update(self, dt)
    local input = self.input
    local bounds = config.world
    self.x = clamp(self.x + input:axis("move_x") * self.speed * dt, 0, bounds.width - self.size)
    self.y = clamp(self.y + input:axis("move_y") * self.speed * dt, 0, bounds.height - self.size)
end
