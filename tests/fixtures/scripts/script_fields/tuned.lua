entity "tuned"

sprite = "tuned.png"
tags = { "enemy" }

health = 3
speed = 2.5
ratio = 3.0
alive = true
title = "boss"
aim = { x = 1, y = 2 }
tint = { r = 255, g = 0, b = 0 }
glow = { r = 1, g = 2, b = 3, a = 4 }
steps = { 1, 2, 3 }
weights = { 1, 2.5 }
names = { "a", "b" }
waypoints = { { x = 0, y = 0 }, { x = 4, y = 5 } }

_secret = 1
empty = {}
nested = { { 1 } }
mixed = { 1, "a" }
map = { a = 1 }
point3 = { x = 1, y = 2, z = 3 }
bad_tint = { r = 300, g = 0, b = 0 }
huge = math.huge

server = { server_only = 9 }

function wound(self)
    self.health = 1
    self.aim = { x = 5, y = 6 }
end

function rename_first(self)
    self.names[1] = "z"
end

function heal_as_float(self)
    self.health = 3.0
end

function break_health(self)
    self.health = 2.5
end

function check_spawned(self)
    assert(self.aim.x == 7 and self.aim.y == 8)
    assert(#self.steps == 1 and self.steps[1] == 4)
end
