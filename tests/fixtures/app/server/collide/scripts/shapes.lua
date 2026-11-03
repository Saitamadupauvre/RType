local function vanish(self)
    self:destroy()
end

entity "bullet" {
    tags = { "bullet" },
    hitbox = { w = 20, h = 20 },
    collides_with = { "target" },
    on_collide = vanish,
}

entity "target" {
    tags = { "target" },
    hitbox = { w = 40, h = 40 },
    collides_with = { "bullet" },
    on_collide = vanish,
}
