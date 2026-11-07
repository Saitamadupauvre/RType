score = 10

cvar.register { name = "game.lives", default = 3, min = 0, max = 9, description = "lives at start" }
cvar.register { name = "game.speed", default = 1, kind = "number", description = "scroll speed" }

function on_start() end

function lives()
    return cvar.get("game.lives")
end
