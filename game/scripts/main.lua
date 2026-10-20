-- TEMPORARY(#72): demo square moved by each player's input. Replace with the real game (sprint 2).

local PLAYER_COLORS = {
    [0] = { r = 80, g = 160, b = 255 },
    [1] = { r = 255, g = 90, b = 90 },
    [2] = { r = 90, g = 220, b = 120 },
    [3] = { r = 255, g = 210, b = 70 },
}

entity "square" {
    tags  = { "player" },
    speed = 500,

    update = function(self, dt)
        local input = self.input
        self.x = self.x + input:axis("move_x") * self.speed * dt
        self.y = self.y + input:axis("move_y") * self.speed * dt
    end,
}

function on_start()
end

function on_player_joined(player)
    local square = spawn("square", { x = 100, y = 120 + player.slot * 140, owner = player })
    square.color = PLAYER_COLORS[player.slot % 4]
end

function on_player_left(player)
    for _, owned in ipairs(player:entities()) do
        owned:destroy()
    end
end
