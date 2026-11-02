local controls = {}

local actions = {
    fire = { keys = { "Space" } },
}

local axes = {
    move_x = { negative = { "Left", "Q" }, positive = { "Right", "D" } },
    move_y = { negative = { "Up", "Z" }, positive = { "Down", "S" } },
}

local function any_key(input, keys)
    for _, key in ipairs(keys) do
        if input:key(key) then
            return true
        end
    end
    return false
end

local function any_button(input, buttons)
    for _, button in ipairs(buttons or {}) do
        if input:mouse(button) then
            return true
        end
    end
    return false
end

function controls.pressed(input, action)
    local binding = actions[action]
    return any_key(input, binding.keys or {}) or any_button(input, binding.mouse)
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
