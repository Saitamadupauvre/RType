event "hit" { at = "vec2", damage = "int" }
event "quiet" {}

function on_start() end

function on_scene_loaded(name)
    scene_hook(name)
end
