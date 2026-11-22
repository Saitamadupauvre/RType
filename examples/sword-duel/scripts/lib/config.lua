return {
    world = {
        width = 1920,
        height = 1080,
        ground_y = 560,
    },
    fighter = {
        speed = 260,
        hitbox = { w = 90, h = 220 },
        dash_speed = 820,
        dash_duration = 0.18,
        dash_cooldown = 0.55,
        reach = 70,
        parry_duration = 0.35,
        parry_lockout = 0.9,
        spawn_x = { 540, 1380 - 90 },
        start_y = 560 - 220,
    },
    match = {
        points_to_win = 3,
        round_reset_delay = 1.2,
    },
    score_board = {
        digit_scale = 3,
        y = 60,
        x = { 90, 1680 },
    },
}
