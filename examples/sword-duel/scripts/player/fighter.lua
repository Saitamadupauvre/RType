local config = require("config")
local controls = require("controls")
local roster = require("roster")

entity "fighter"

persistent = true
tags = { "fighter" }
sprite = "sprites/fighter.png"
hitbox = config.fighter.hitbox
collides_with = { "fighter" }
color = { r = 255, g = 255, b = 255 }
scale = { x = 1, y = 1 }

state = "idle"
facing = 1
slot = 1
vx = 0
dash_timer = 0
dash_cooldown = 0
parry_timer = 0
attack_lockout = 0
mirror = nil

local function clamp(value, low, high)
    return math.max(low, math.min(value, high))
end

function on_spawn(self)
    roster.add(self, "fighters")
    self.mirror = spawn("mirror", { x = self.x, y = self.y })
end

function on_destroy(self)
    roster.remove(self)
    if self.mirror then
        self.mirror:destroy()
        self.mirror = nil
    end
end

function opponent(self)
    for _, other in ipairs(roster.list("fighters")) do
        if other.id ~= self.id then
            return other
        end
    end
    return nil
end

function set_tint(self)
    if self.state == "parrying" then
        self.color = { r = 150, g = 230, b = 255 }
    elseif self.state == "dashing" then
        self.color = { r = 255, g = 255, b = 160 }
    elseif self.state == "recover" then
        self.color = { r = 140, g = 140, b = 140 }
    else
        self.color = { r = 255, g = 255, b = 255 }
    end
end

function spawn_ripple(self)
    spawn("ripple", {
        x = self.x + config.fighter.hitbox.w / 2 - 16,
        y = config.world.ground_y - 8,
    })
end

function update_mirror(self)
    if not self.mirror then
        return
    end
    local ground = config.world.ground_y
    self.mirror.x = self.x
    self.mirror.y = ground + (ground - (self.y + config.fighter.hitbox.h))
    self.mirror.scale = { x = self.facing, y = -1 }
end

function update(self, dt)
    local input = self.input
    local opp = self:opponent()

    if opp then
        self.facing = opp.x >= self.x and 1 or -1
    end
    self.scale = { x = self.facing, y = 1 }

    self.dash_cooldown = math.max(0, self.dash_cooldown - dt)
    self.attack_lockout = math.max(0, self.attack_lockout - dt)

    if self.state == "dashing" then
        self.dash_timer = self.dash_timer - dt
        if self.dash_timer <= 0 then
            self.state = "recover"
            self.vx = 0
            self.dash_timer = 0.15
        end
    elseif self.state == "recover" then
        self.dash_timer = self.dash_timer - dt
        if self.dash_timer <= 0 then
            self.state = "idle"
        end
    elseif self.state == "parrying" then
        self.parry_timer = self.parry_timer - dt
        if self.parry_timer <= 0 or not controls.parry(input) then
            self.state = "idle"
            self.attack_lockout = config.fighter.parry_lockout
        end
    else
        self.state = "idle"
        local moved = controls.axis(input, "move_x")
        if moved ~= 0 then
            self.x = clamp(self.x + moved * config.fighter.speed * dt, 0, config.world.width - self.hitbox.w)
            self.spawn_timer = (self.spawn_timer or 0) - dt
            if self.spawn_timer == nil or self.spawn_timer <= 0 then
                self:spawn_ripple()
                self.spawn_timer = 0.12
            end
        end

        if controls.parry(input) and self.attack_lockout <= 0 then
            self.state = "parrying"
            self.parry_timer = config.fighter.parry_duration
        elseif controls.dash(input) and self.dash_cooldown <= 0 and self.attack_lockout <= 0 then
            self.state = "dashing"
            self.dash_timer = config.fighter.dash_duration
            self.dash_cooldown = config.fighter.dash_cooldown
            self.vx = self.facing * config.fighter.dash_speed
        end
    end

    if self.state ~= "dashing" then
        self.vx = 0
    end

    self.x = clamp(self.x, 0, config.world.width - self.hitbox.w)
    self:set_tint()
    self:update_mirror()
end

function land_hit(self, defender)
    local match = roster.list("match")[1]
    if match then
        match:score_point(self.slot)
    end
end

function on_collide(self, other)
    if not other:has_tag("fighter") or self.state ~= "dashing" then
        return
    end

    self.vx = 0
    if other.state == "parrying" then
        self.state = "recover"
        self.dash_timer = 0.25
    else
        self.state = "recover"
        self.dash_timer = 0.25
        self:land_hit(other)
    end
end
