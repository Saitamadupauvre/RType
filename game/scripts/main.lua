local roster = require("roster")

local function game()
    return roster.list("game")[1]
end

function on_start()
    spawn("game")
end

function on_player_joined(player)
    game():join(player)
end

function on_player_left(player)
    game():leave(player)
end
