local keys = {}
for index = 1, 17 do
    keys["key" .. index] = "bool"
end
event "crowded" (keys)
