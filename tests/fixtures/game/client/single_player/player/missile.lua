local config = require("config")

entity "missile"

tags  = { "missile" }
speed = config.missile.speed
size  = config.missile.size

function update(self, dt)
    self.x = self.x + self.speed * dt
    if self.x >= config.world.width then
        self:destroy()
    end
end
