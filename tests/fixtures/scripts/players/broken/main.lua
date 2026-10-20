function on_player_joined(player)
    error("join failed for " .. player.id)
end

function on_player_left(player)
    error("leave failed for " .. player.id)
end
