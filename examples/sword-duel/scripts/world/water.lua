local config = require("config")

entity "water"

local square_size = 64

shader = "shaders/water"
color = { r = 0, g = 0, b = 0 }
z_index = -60
scale = { x = config.world.width / square_size, y = (config.world.height - config.world.ground_y) / square_size }
