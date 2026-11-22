local controls = {}

local axes = {
    move_y = { negative = { "Up", "W" }, positive = { "Down", "S" } },
}

local function any_key(input, keys)
    for _, key in ipairs(keys) do
        if input:key(key) then
            return true
        end
    end
    return false
end

function controls.axis(input, axis)
    local binding = axes[axis]
    local value = 0
    if any_key(input, binding.positive) then
        value = value + 1
    end
    if any_key(input, binding.negative) then
        value = value - 1
    end
    return value
end

return controls
