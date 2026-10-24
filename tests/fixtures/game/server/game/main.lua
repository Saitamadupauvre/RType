local roster = require("roster")

sounds { "laser_shoot.wav", "explosion.wav" }
musics { "bit_space.mp3" }

local function game()
    return roster.list("game")[1]
end

function on_start()
end

function on_player_joined(player)
    game():join(player)
end

function on_player_left(player)
    game():leave(player)
end
