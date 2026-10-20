-- TEMPORARY(#72): demo square moved by each player's input. Replace with the real game (sprint 2).

entity "square" {
    tags  = { "player" },
    color = { r = 255, g = 255, b = 255 },
    speed = 500,

    update = function(self, dt)
        local input = self.input
        self.x = self.x + input:axis("move_x") * self.speed * dt
        self.y = self.y + input:axis("move_y") * self.speed * dt
    end,
}
