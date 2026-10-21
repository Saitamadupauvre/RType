local aabb = {}

function aabb.overlaps(a, b)
    return a.x < b.x + b.size and b.x < a.x + a.size
       and a.y < b.y + b.size and b.y < a.y + a.size
end

return aabb
