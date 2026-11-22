entity "ripple"

persistent = false
sprite = "sprites/ripple.png"
color = { r = 140, g = 170, b = 200 }
z_index = -40
life = 0.5
scale = { x = 1, y = 2.4 }

function update(self, dt)
    self.life = self.life - dt
    local grow = 1 + (0.5 - self.life) * 3
    self.scale = { x = grow, y = grow * 2.4 }
    if self.life <= 0 then
        self:destroy()
    end
end
