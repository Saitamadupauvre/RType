-- TEMPORARY(#72): demo to try the client loop. Replace with the real game (sprint 2).

local SIZE = 64
local WHITE = { r = 255, g = 255, b = 255 }
local RED = { r = 230, g = 40, b = 40 }
local target

local function overlaps(a, b)
    return a.x < b.x + SIZE and b.x < a.x + SIZE
       and a.y < b.y + SIZE and b.y < a.y + SIZE
end

entity "square" {
    tags  = { "player" },
    color = WHITE,
    speed = 500,

    update = function(self, dt)
        local input = self.input
        self.x = self.x + input:axis("move_x") * self.speed * dt
        self.y = self.y + input:axis("move_y") * self.speed * dt
        if target and overlaps(self, target) then
            self.color = RED
        else
            self.color = WHITE
        end
    end,
}

entity "block" {
    color = { r = 60, g = 120, b = 230 },
}

function on_start()
    spawn("square", { x = 400, y = 500 })
    target = spawn("block", { x = 1200, y = 500 })
end
