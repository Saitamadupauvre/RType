local aabb = {}

function aabb.overlaps(a, b)
    return a.x < b.x + b.hitbox.w and b.x < a.x + a.hitbox.w
       and a.y < b.y + b.hitbox.h and b.y < a.y + a.hitbox.h
end

return aabb
