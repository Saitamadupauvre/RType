entity "broken"

tags          = { "prey" }
hitbox        = { w = 10, h = 10 }
collides_with = { "hunter" }

function on_collide(self, other)
    error("collision exploded")
end
