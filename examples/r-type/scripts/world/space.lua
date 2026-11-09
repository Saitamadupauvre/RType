local config = require("config")

entity "space"

local square_size = 64

shader = "shaders/space"
color  = { r = 0, g = 0, b = 0 }
scale  = { x = config.world.width / square_size, y = config.world.height / square_size }
