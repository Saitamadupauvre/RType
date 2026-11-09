entity "enemy"

sprite = "enemy.png"
health = 1

function update(self, dt)
    self.x = self.x - dt
end
