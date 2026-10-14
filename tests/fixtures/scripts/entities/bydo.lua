entity "bydo"

x = 800
vx = -150
health = 2

function damage(self, amount)
    self.health = self.health - amount
end

function explode(self)
    error("bydo exploded")
end

function heal(self)
    self.health = self.health + 1
end
