return {
    world = {
        width = 1920,
        height = 1080,
    },
    paddle = {
        speed = 620,
        hitbox = { w = 28, h = 160 },
        spawn_x = { 60, 1920 - 60 - 28 },
        start_y = (1080 - 160) / 2,
    },
    ball = {
        hitbox = { w = 28, h = 28 },
        start_speed = 520,
        speed_gain = 1.07,
        max_speed = 1400,
        start_y = (1080 - 28) / 2,
    },
    match = {
        points_to_win = 11,
        round_reset_delay = 1.0,
    },
    score_board = {
        digit_scale = 3,
        y = 60,
        x = { 760, 1130 },
    },
}
