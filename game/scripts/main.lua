local roster = require("roster")

sounds { "laser_shoot.wav", "explosion.wav" }
musics { "bit_space.mp3" }

local function game()
    return roster.list("game")[1]
end

server = {
    on_start = function()
        spawn("game")
        music("bit_space.mp3")
    end,

    on_player_joined = function(player)
        game():join(player)
    end,

    on_player_left = function(player)
        game():leave(player)
    end,
}

client = {
    update = function(dt)
        if input:pressed("M") then
            audio.set_muted(not audio.muted())
        end
    end,
}
