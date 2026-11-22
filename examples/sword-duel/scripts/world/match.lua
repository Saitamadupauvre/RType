local config = require("config")
local roster = require("roster")

entity "match"

persistent = true
state = "waiting"
scores = { 0, 0 }
reset_timer = 0
fighters = { nil, nil }
digits = { nil, nil }

function on_spawn(self)
    roster.add(self, "match")
    for slot = 1, 2 do
        self.digits[slot] = spawn("score_digit", {
            x = config.score_board.x[slot],
            y = config.score_board.y,
        })
    end
end

function spawn_fighter(self, player, slot)
    local fighter = spawn("fighter", {
        x = config.fighter.spawn_x[slot],
        y = config.fighter.start_y,
        owner = player,
    })
    fighter.slot = slot
    self.fighters[slot] = fighter
    return fighter
end

function join(self, player)
    if not self.fighters[1] then
        self:spawn_fighter(player, 1)
    elseif not self.fighters[2] then
        self:spawn_fighter(player, 2)
        self.state = "playing"
    end
end

function leave(self, player)
    for _, owned in ipairs(player:entities()) do
        if owned:has_tag("fighter") then
            for slot = 1, 2 do
                if self.fighters[slot] and self.fighters[slot].id == owned.id then
                    self.fighters[slot] = nil
                end
            end
            roster.destroy(owned)
            self.state = "waiting"
        end
    end
end

function reset_round(self)
    for slot = 1, 2 do
        local fighter = self.fighters[slot]
        if fighter then
            fighter.x = config.fighter.spawn_x[slot]
            fighter.y = config.fighter.start_y
            fighter.state = "idle"
            fighter.vx = 0
            fighter.dash_cooldown = 0
            fighter.attack_lockout = 0
        end
    end
end

function restart_match(self)
    self.scores = { 0, 0 }
    for slot = 1, 2 do
        self.digits[slot]:set_value(0)
    end
    self:reset_round()
    self.state = "playing"
end

function score_point(self, slot)
    if self.state ~= "playing" then
        return
    end
    self.scores[slot] = self.scores[slot] + 1
    self.digits[slot]:set_value(self.scores[slot])

    if self.scores[slot] >= config.match.points_to_win then
        self.state = "match_over"
        self.reset_timer = config.match.round_reset_delay * 3
    else
        self.state = "point_scored"
        self.reset_timer = config.match.round_reset_delay
    end
end

function update(self, dt)
    if self.state == "point_scored" then
        self.reset_timer = self.reset_timer - dt
        if self.reset_timer <= 0 then
            self:reset_round()
            self.state = "playing"
        end
    elseif self.state == "match_over" then
        self.reset_timer = self.reset_timer - dt
        if self.reset_timer <= 0 then
            self:restart_match()
        end
    end
end
