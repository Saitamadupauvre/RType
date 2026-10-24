entity "hunter"

tags          = { "hunter" }
hitbox        = { w = 10, h = 20 }
collides_with = { "prey" }

function on_collide(self, other)
    self.x = self.x + 1
    self.y = other.id
end

function measure(self)
    self.x = self.hitbox.w
    self.y = self.hitbox.h
end

function count_filters(self)
    self.x = #self.collides_with
end

function hunt_duelists(self)
    self.collides_with = { "duelist" }
end
