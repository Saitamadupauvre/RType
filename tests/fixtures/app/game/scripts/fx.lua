entity "spark" {
    color = { r = 200, g = 100, b = 0 },
    client = {
        update = function(self, dt)
            self.x = self.x + 1
        end,
    },
}

entity "dust" {
    color = { r = 90, g = 90, b = 90 },
    vx = 60,
}
