local config = require("config")

entity "sky"

local square_size = 64

shader = "shaders/mono_sky"
color = { r = 0, g = 0, b = 0 }
z_index = -200
scale = { x = config.world.width / square_size, y = config.world.height / square_size }
