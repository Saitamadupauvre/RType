local config = require("config")
local roster = require("roster")

entity "game"

tags          = { "player" }
state         = "playing"
restart_armed = false

function on_spawn(self)
    spawn("collisions")
    self:start()
end

function start(self)
    spawn("ship", { x = config.ship.start.x, y = config.ship.start.y })
    self.spawner = spawn("bydo_spawner")
    self.state = "playing"
end

function finish(self)
    self.spawner:destroy()
    self.spawner = nil
    self.restart_armed = false
    self.state = "game_over"
end

function clear(self)
    for _, group in ipairs({ "missiles", "enemies" }) do
        for _, member in ipairs(roster.list(group)) do
            roster.destroy(member)
        end
    end
end

function restart(self)
    self:clear()
    self:start()
end

function wait_for_restart(self)
    if not self.input:pressed("fire") then
        self.restart_armed = true
    elseif self.restart_armed then
        self:restart()
    end
end

function update(self, dt)
    if self.state == "game_over" then
        self:wait_for_restart()
    elseif #roster.list("players") == 0 then
        self:finish()
    end
end
