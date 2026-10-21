local config = require("config")

entity "bydo_spawner"

elapsed = 0

function spawn_bydo(self)
    spawn("bydo", {
        x = config.world.width,
        y = math.random(0, config.world.height - config.bydo.size),
    })
end

function update(self, dt)
    local interval = config.bydo.spawn_interval
    self.elapsed = self.elapsed + dt
    while self.elapsed >= interval do
        self.elapsed = self.elapsed - interval
        self:spawn_bydo()
    end
end
