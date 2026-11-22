local config = require("config")
local roster = require("roster")

entity "ball"

persistent = true
tags = { "ball" }
sprite = "sprites/ball.png"
hitbox = config.ball.hitbox
collides_with = { "paddle" }
color = { r = 255, g = 255, b = 255 }

function on_spawn(self)
    roster.add(self, "ball")
    self:serve(math.random(0, 1) == 0 and -1 or 1)
end

function on_destroy(self)
    roster.remove(self)
end

function serve(self, direction)
    self.x = config.world.width / 2 - self.hitbox.w / 2
    self.y = config.ball.start_y
    self.vx = direction * config.ball.start_speed
    self.vy = (math.random(0, 1) == 0 and -1 or 1) * config.ball.start_speed * 0.4
end

function update(self, dt)
    if self.y <= 0 then
        self.y = 0
        self.vy = math.abs(self.vy)
    elseif self.y >= config.world.height - self.hitbox.h then
        self.y = config.world.height - self.hitbox.h
        self.vy = -math.abs(self.vy)
    end

    local match = roster.list("match")[1]
    if not match then
        return
    end

    if self.x + self.hitbox.w < 0 then
        match:score_point(2)
        self:serve(-1)
    elseif self.x > config.world.width then
        match:score_point(1)
        self:serve(1)
    end
end

function on_collide(self, other)
    if not other:has_tag("paddle") then
        return
    end

    local paddle_center = other.y + other.hitbox.h / 2
    local ball_center = self.y + self.hitbox.h / 2
    local offset = (ball_center - paddle_center) / (other.hitbox.h / 2)
    offset = math.max(-1, math.min(1, offset))

    local speed = math.min(math.abs(self.vx) * config.ball.speed_gain, config.ball.max_speed)
    local direction = self.vx < 0 and 1 or -1

    self.x = other.slot == 1 and (other.x + other.hitbox.w) or (other.x - self.hitbox.w)
    self.vx = direction * speed
    self.vy = offset * speed * 0.75
end
