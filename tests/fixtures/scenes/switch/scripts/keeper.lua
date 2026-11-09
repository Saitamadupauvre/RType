entity "keeper"

persistent = true
count = 0

function bump(self)
    self.count = self.count + 1
end

function check_bumped(self)
    assert(self.count == 1)
end
