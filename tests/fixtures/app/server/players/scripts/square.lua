entity "square" {
    speed = 60,
    color = { r = 10, g = 20, b = 30 },

    update = function(self, dt)
        self.x = self.x + (self.input:key("Right") and 1 or 0) * self.speed * dt
        self.y = self.y + (self.input:key("Down") and 1 or 0) * self.speed * dt
    end,
}

entity "scenery" {}
