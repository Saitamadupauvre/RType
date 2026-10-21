local config = require("config")

entity "bydo"

tags  = { "enemy" }
color = { r = 230, g = 70, b = 70 }
speed = config.bydo.speed
size  = config.bydo.size

function update(self, dt)
    self.x = self.x - self.speed * dt
    if self.x <= -self.size then
        self:destroy()
    end
end
