function on_player_joined(player)
    spawn("square", { x = 100, y = player.slot * 100, owner = player })
end
