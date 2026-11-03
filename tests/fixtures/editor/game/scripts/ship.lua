entity "ship"

sprite = "ship.png"
color = { r = 10, g = 20, b = 30 }

health = 3

function on_spawn(self)
    error("on_spawn ran in the editor")
end

function update(self, dt)
    error("update ran in the editor")
end
