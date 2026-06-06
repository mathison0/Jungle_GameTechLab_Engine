local slash = nil

local forwardOffset = 2.5
local heightOffset = 1.2

function BeginPlay()
    local actor = World.FindFirstActorByTag("SlashEffect")
    if actor ~= nil then
        slash = actor:AsSlashEffectActor()
    end
end

function Tick(dt)
    if slash == nil then
        return
    end

    if Input.GetKeyDown(Key.Space) then
        local forward = obj.Forward
        local loc = obj.Location

        slash:ActivateSlash(loc, obj.Rotation, forward)
    end
end