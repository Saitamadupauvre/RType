entity "prey"

tags   = { "prey" }
hitbox = { w = 10, h = 10 }

function on_collide(self, other)
    self.x = self.x + 1
end

function become_duelist(self)
    self.tags = { "duelist" }
end
