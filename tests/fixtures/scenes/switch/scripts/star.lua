entity "star"

function on_spawn(self)
    if spawn_hook then
        spawn_hook(self.type)
    end
end
