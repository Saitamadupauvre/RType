local config = require("config")
local roster = require("roster")

entity "match"

persistent = true
state = "waiting"
scores = { 0, 0 }
reset_timer = 0
paddles = { nil, nil }
digits = { nil, nil }
ball = nil

function on_spawn(self)
    roster.add(self, "match")

    for slot = 1, 2 do
        self.digits[slot] = spawn("score_digit", {
            x = config.score_board.x[slot],
            y = config.score_board.y,
        })
    end

    local dash_count = 16
    local gap = config.world.height / dash_count
    for i = 0, dash_count - 1 do
        spawn("net_dash", {
            x = config.world.width / 2 - 4,
            y = i * gap,
        })
    end

    self.ball = spawn("ball")
end

function spawn_paddle(self, player, slot)
    local paddle = spawn("paddle", {
        x = config.paddle.spawn_x[slot],
        y = config.paddle.start_y,
        owner = player,
    })
    paddle.slot = slot
    self.paddles[slot] = paddle
    return paddle
end

function join(self, player)
    if not self.paddles[1] then
        self:spawn_paddle(player, 1)
    elseif not self.paddles[2] then
        self:spawn_paddle(player, 2)
        self.state = "playing"
    end
end

function leave(self, player)
    for _, owned in ipairs(player:entities()) do
        if owned:has_tag("paddle") then
            for slot = 1, 2 do
                if self.paddles[slot] and self.paddles[slot].id == owned.id then
                    self.paddles[slot] = nil
                end
            end
            roster.destroy(owned)
            self.state = "waiting"
        end
    end
end

function reset_round(self)
    for slot = 1, 2 do
        local paddle = self.paddles[slot]
        if paddle then
            paddle.y = config.paddle.start_y
        end
    end
    if self.ball then
        self.ball:serve(math.random(0, 1) == 0 and -1 or 1)
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
            self.state = "playing"
        end
    elseif self.state == "match_over" then
        self.reset_timer = self.reset_timer - dt
        if self.reset_timer <= 0 then
            self:restart_match()
        end
    end
end
