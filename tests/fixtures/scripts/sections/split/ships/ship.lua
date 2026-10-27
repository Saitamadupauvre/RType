entity "ship" {
    sprite = "ship.png",

    server = {
        speed = 400,
        update = function(self, dt)
            self.ticks = (self.ticks or 0) + 1
        end,
    },

    client = {
        speed = 10,
        glow = true,
    },

    check_server = function(self)
        assert(self.speed == 400 and self.glow == nil)
        assert(self.server == nil and self.client == nil)
    end,

    check_client = function(self)
        assert(self.speed == 10 and self.glow == true)
        assert(self.server == nil and self.client == nil)
    end,

    check_core = function(self)
        assert(self.speed == nil and self.glow == nil)
        assert(self.server == nil and self.client == nil)
        assert(self.sprite == "ship.png")
    end,

    check_two_ticks = function(self)
        assert(self.ticks == 2)
    end,

    check_no_ticks = function(self)
        assert(self.ticks == nil)
    end,
}
