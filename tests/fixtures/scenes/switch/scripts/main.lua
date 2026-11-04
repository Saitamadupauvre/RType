server = {
    on_start = function() end,

    on_scene_loaded = function(name)
        if scene_hook then
            scene_hook(name)
        end
    end,
}
