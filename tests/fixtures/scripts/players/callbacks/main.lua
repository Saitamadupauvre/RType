function on_player_joined(player)
    spawn("ship", { x = 0, y = player.slot * 100, owner = player })
end

function on_player_left(player)
    for _, ship in ipairs(player:entities()) do
        ship:destroy()
    end
    spawn("marker", { y = player.id })
end
