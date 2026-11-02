event "ping" { n = "int" }
textures { "backgrounds/stars.png", "ui/bar.png", "ui/missing.png", "enemies/bug.png", "enemies/boss.png" }
sounds { "shot.wav" }
musics { "stage.ogg" }

server = {
    on_start = function() end,
    on_player_joined = function(player)
        spawn("square", { x = 10, y = 20, owner = player })
        emit("ping", { n = 7 })
    end,
}

client = {
    on_start = function()
        on("ping", function(event)
            _G.pinged = event.n
        end)
    end,
    update = function(dt)
        if input:pressed("M") then
            audio.set_muted(not audio.muted())
        end
    end,
}
