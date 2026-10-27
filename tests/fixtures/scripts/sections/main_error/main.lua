_G.calls = 0

function update(dt)
    _G.calls = _G.calls + 1
    error("broken update")
end
