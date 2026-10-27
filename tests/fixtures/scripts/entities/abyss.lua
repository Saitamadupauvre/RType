entity "abyss"

deep = {}
local cursor = deep
for _ = 1, 100 do
    cursor.next = {}
    cursor = cursor.next
end
