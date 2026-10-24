local config = require("config")
local roster = require("roster")

entity "game"

state     = "waiting"
countdown = 0

function on_spawn(self)
    roster.add(self, "game")
end

function spawn_ship(self, player)
    local start = config.ship.start
    local colors = config.ship.colors
    local row = player.slot - (#colors - 1) / 2
    local ship = spawn("ship", {
        x = start.x,
        y = start.y + row * config.ship.row_spacing,
        owner = player,
    })
    ship.color = colors[player.slot % #colors + 1]
end

function start(self)
    for _, player in ipairs(players()) do
        self:spawn_ship(player)
    end
    self.spawner = spawn("bydo_spawner")
    self.state = "playing"
end

function finish(self)
    self.spawner:destroy()
    self.spawner = nil
    self.countdown = config.game.restart_delay
    self.state = "game_over"
end

function clear(self)
    for _, group in ipairs({ "missiles", "enemies" }) do
        for _, member in ipairs(roster.list(group)) do
            roster.destroy(member)
        end
    end
end

function join(self, player)
    if self.state == "waiting" then
        self:start()
    elseif self.state == "playing" then
        self:spawn_ship(player)
    end
end

function leave(self, player)
    for _, owned in ipairs(player:entities()) do
        roster.destroy(owned)
    end
end

function restart(self)
    self:clear()
    if #players() == 0 then
        self.state = "waiting"
    else
        self:start()
    end
end

function update(self, dt)
    if self.state == "game_over" then
        self.countdown = self.countdown - dt
        if self.countdown <= 0 then
            self:restart()
        end
    elseif self.state == "playing" and #roster.list("players") == 0 then
        self:finish()
    end
end
