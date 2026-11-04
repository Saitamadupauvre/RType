entity "spark" {
    x = 10,
    y = 20,
    sprite = "fx/spark.png",
    lifetime = 0,
    server = {
        update = function(self, dt)
            self.lifetime = -1
        end,
    },
    client = {
        update = function(self, dt)
            self.x = self.x + 1
            self.lifetime = self.lifetime + 1
            if self.lifetime >= 3 then
                self:destroy()
            end
        end,
    },
}

