local config = require("config")

function on_start()
    spawn("ship", { x = config.ship.start.x, y = config.ship.start.y })
end
