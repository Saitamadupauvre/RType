return {
    world = { width = 1920, height = 1080 },
    ship = {
        hitbox = { w = 96, h = 36 },
        speed = 500,
        start = { x = 160, y = 522 },
        row_spacing = 160,
        fire_cooldown = 0.25,
        colors = {
            { r = 80, g = 160, b = 255 },
            { r = 255, g = 90, b = 90 },
            { r = 90, g = 220, b = 120 },
            { r = 255, g = 210, b = 70 },
        },
    },
    missile = { hitbox = { w = 48, h = 24 }, speed = 1200 },
    bydo = {
        hitbox = { w = 63, h = 72 },
        speed = 300,
        spawn_interval = 1.5,
        first_shot_delay = 0.8,
        fire_interval = 2.5,
    },
    bydo_shot = { hitbox = { w = 24, h = 24 }, speed = 700 },
    game = { restart_delay = 3 },
}
