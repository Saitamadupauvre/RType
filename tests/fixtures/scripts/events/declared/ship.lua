entity "ship"

function blow_up(self)
    emit("explosion", { at = { x = 1, y = -2 }, size = 2 })
end
