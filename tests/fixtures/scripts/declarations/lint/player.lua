entity "player" {
    speed = 300,
}

speed_bonus = 2

function update(self, dt)
    self.x = self.x + self.speed * dt
end
