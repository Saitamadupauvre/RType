entity "square" {
    speed = 60,

    update = function(self, dt)
        self.x = self.x + self.input:axis("move_x") * self.speed * dt
        self.y = self.y + self.input:axis("move_y") * self.speed * dt
    end,
}
