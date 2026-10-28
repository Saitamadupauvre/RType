entity "ship" {
    tags = { "ship" },

    update = function(self, dt)
        self.x = self.x + (self.input:key("Right") and 1 or 0) * dt
    end,
}

entity "marker" {
    tags = { "marker" },
}
