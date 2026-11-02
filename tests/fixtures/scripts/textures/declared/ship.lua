entity "ship"

sprite = "sprites/ship.png"

function wear_boss(self)
    self.sprite = "sprites/boss/core.png"
end

function wear_unknown(self)
    self.sprite = "sprites/unknown.png"
end

function wear_nothing(self)
    self.sprite = ""
end
