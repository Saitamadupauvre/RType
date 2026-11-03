entity "backdrop" {
    shader = "shaders/nebula",

    use_wave = function(self)
        self.shader = "shaders/fx/wave"
    end,

    use_nothing = function(self)
        self.shader = ""
    end,

    use_unknown = function(self)
        self.shader = "shaders/unknown"
    end,

    use_texture = function(self)
        self.sprite = "shaders/space"
    end,
}
