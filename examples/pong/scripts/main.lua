local roster = require("roster")

local function match()
    return roster.list("match")[1]
end

server = {
    on_player_joined = function(player)
        match():join(player)
    end,

    on_player_left = function(player)
        match():leave(player)
    end,
}
