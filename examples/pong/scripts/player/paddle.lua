local config = require("config")
local controls = require("controls")
local roster = require("roster")

entity "paddle"

persistent = true
tags = { "paddle" }
sprite = "sprites/paddle.png"
hitbox = config.paddle.hitbox
color = { r = 255, g = 255, b = 255 }

slot = 1

local function clamp(value, low, high)
    return math.max(low, math.min(value, high))
end

function on_spawn(self)
    roster.add(self, "paddles")
end

function on_destroy(self)
    roster.remove(self)
end

function update(self, dt)
    local input = self.input
    local moved = controls.axis(input, "move_y")
    if moved ~= 0 then
        self.y = clamp(self.y + moved * config.paddle.speed * dt, 0, config.world.height - self.hitbox.h)
    end
end
