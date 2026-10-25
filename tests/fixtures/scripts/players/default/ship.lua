entity "ship" {
    tags = { "ship" },

    update = function(self, dt)
        self.x = self.x + self.input:axis("move_x") * dt
    end,
}

entity "marker" {
    tags = { "marker" },
}
