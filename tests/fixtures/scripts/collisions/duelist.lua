entity "duelist"

tags          = { "duelist" }
hitbox        = { w = 10, h = 10 }
collides_with = { "duelist" }

function on_collide(self, other)
    self.x = self.x + 1
end
