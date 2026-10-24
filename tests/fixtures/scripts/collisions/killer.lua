entity "killer"

tags          = { "prey" }
hitbox        = { w = 10, h = 10 }
collides_with = { "hunter" }

function on_collide(self, other)
    self.x = self.x + 1
    other:destroy()
end
