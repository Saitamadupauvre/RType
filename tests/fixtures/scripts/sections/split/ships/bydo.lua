entity "bydo"

health = 1

server = {
    points = 100,
}

client = {
    trail = "red",
}

function check_server(self)
    assert(self.health == 1 and self.points == 100 and self.trail == nil)
end

function check_client(self)
    assert(self.health == 1 and self.points == nil and self.trail == "red")
end
