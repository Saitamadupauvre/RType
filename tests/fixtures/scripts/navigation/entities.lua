entity "wall" {
    tags = { "wall" },
    hitbox = { w = 40, h = 160 },
}

entity "hazard" {
    tags = { "hazard" },
    hitbox = { w = 40, h = 40 },
}

entity "walker" {
    hitbox = { w = 10, h = 10 },
    on_arrive = function(self)
        self.arrived = true
    end,
}

entity "ghost" {
}
