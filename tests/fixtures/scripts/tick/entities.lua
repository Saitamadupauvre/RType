entity "mover" {
    update = function(self, dt)
        self.x = self.x + dt * 10
        self.y = self.age
    end,
}

entity "greeter" {
    on_spawn = function(self)
        self.y = 42
    end,
}

entity "parent" {
    update = function(self, dt)
        spawn("greeter")
    end,
}

entity "doomed" {
    update = function(self, dt)
        self:destroy()
        self:destroy()
        self.x = 7
    end,

    on_destroy = function(self)
        spawn("marker")
    end,
}

entity "chain" {
    on_destroy = function(self)
        spawn("doomed_now"):destroy()
    end,

    die = function(self)
        self:destroy()
    end,
}

entity "doomed_now" {
    on_destroy = function(self)
        spawn("marker")
    end,
}

entity "marker" {
    tags = { "marker" },
}

entity "faulty" {
    update = function(self, dt)
        error("faulty update")
    end,
}

entity "ship" {
    sprite = "ship.png",
    tags   = { "player", "ally", "player" },
    speed  = 100,

    update = function(self, dt)
        self.vx = (self.input:key("Right") and 1 or 0) * self.speed
        self.vy = (self.input:key("Up") and -1 or 0) * self.speed
        self.x = self.input:key("Space") and 5 or 0
        self.y = self:has_tag("player") and 1 or 0
    end,

    count_tags = function(self)
        self.x = #self.tags
    end,

    retag = function(self)
        self.tags = { "enemy" }
        self.sprite = "bydo.png"
    end,

    break_input = function(self)
        self.input = 3
    end,
}

entity "painted" {
    color = { r = 10, g = 20, b = 30 },

    repaint = function(self)
        self.color = { r = 255, g = 0, b = 0, a = 128 }
    end,

    read_color = function(self)
        local color = self.color
        self.x = color.g
        self.y = color.a
    end,

    out_of_range = function(self)
        self.color = { r = 300, g = 0, b = 0 }
    end,

    missing_channel = function(self)
        self.color = { r = 1, g = 2 }
    end,
}

entity "bad_tags" {
    tags = { "ok", 5 },
}

entity "cursor" {
    update = function(self, dt)
        local x, y = self.input:pointer()
        self.x = x or -1
        self.y = y or -1
    end,

    read_click = function(self)
        self.x = self.input:mouse("Left") and 1 or 0
    end,

    read_unknown_key = function(self)
        return self.input:key("Spcae")
    end,

    read_unknown_button = function(self)
        return self.input:mouse("Back")
    end,

    read_key_of_wrong_type = function(self)
        return self.input:key(32)
    end,
}
