entity "player" {
    sprite = "ship.png",
    speed = 400,
}

entity "player_missile" {
    sprite = "missile.png",
    vx = 900,
}

system "respawn" {
    on_destroy = function(e) end,
}
