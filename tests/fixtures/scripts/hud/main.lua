textures { "ui/heart.png" }

client = {
    draw_hud = function()
        if hud then
            hud()
        end
    end,
}
