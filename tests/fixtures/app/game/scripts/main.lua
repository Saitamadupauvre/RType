event "ping" { n = "int" }

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
