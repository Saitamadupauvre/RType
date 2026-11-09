entity "patrol"

waypoints = { { x = 1, y = 2 }, { x = 3, y = 4 } }
settings = { speed = 5, nested = { depth = 1 } }
settings.nested.owner = settings

function forget_first(self)
    table.remove(self.waypoints, 1)
end
