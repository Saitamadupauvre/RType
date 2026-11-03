entity "rock"

persistent = false

function on_destroy(self)
    if destroy_hook then
        destroy_hook(self.type)
    end
end
