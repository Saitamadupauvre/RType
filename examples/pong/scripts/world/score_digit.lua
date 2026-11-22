local config = require("config")

entity "score_digit"

textures {
    "sprites/digit_0.png", "sprites/digit_1.png", "sprites/digit_2.png",
    "sprites/digit_3.png", "sprites/digit_4.png", "sprites/digit_5.png",
    "sprites/digit_6.png", "sprites/digit_7.png", "sprites/digit_8.png",
    "sprites/digit_9.png",
}

persistent = true
sprite = "sprites/digit_0.png"
color = { r = 255, g = 255, b = 255 }
z_index = -150
scale = config.score_board.digit_scale

function set_value(self, value)
    local clamped = math.max(0, math.min(9, value))
    self.sprite = "sprites/digit_" .. tostring(clamped) .. ".png"
end
