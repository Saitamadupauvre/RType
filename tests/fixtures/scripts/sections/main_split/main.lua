_G.started = "none"
_G.ticks = 0

server = {
    on_start = function()
        _G.started = "server"
    end,
}

client = {
    on_start = function()
        _G.started = "client"
    end,
    update = function(dt)
        _G.ticks = _G.ticks + 1
        _G.last_dt = dt
    end,
}
