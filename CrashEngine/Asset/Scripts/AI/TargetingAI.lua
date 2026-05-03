local Co = require("LuaCoroutine")
local Vec = require("Core.Vector")

local TargetingAI = {
    properties = {
        MoveSpeed = { type = "float", default = 1.0, min = 0.1, max = 10.0, speed = 0.05},
        TargetSearchRadius = { type = "float", default = 10000.0, min = 0.0, max = 100000.0, speed = 100.0},
    }
}

function TargetingAI.Target(self, ComponentHandle, TargetTag, bZAxisOnly, DeltaTime)
    if ComponentHandle == nil or not ComponentHandle:IsValid() then
        return
    end

    local myPos = ComponentHandle:GetWorldLocation()
    if self.target == nil or not self.target:IsValid() then
        self.target = self.QueryActorByTagClosest(TargetTag, myPos, self.TargetSearchRadius or 10000.0)
    end

    if self.target ~= nil and self.target:IsValid() then
        local targetPos = self.target:GetLocation()
        if bZAxisOnly then
            targetPos.z = myPos.z
        end

        ComponentHandle:LookAt(targetPos)
    end
end

function TargetingAI.TargetCoroutine(self, ComponentHandle, TargetTag, bZAxisOnly)
    while true do
        local deltaTime = Co.WaitNextFrame()
        TargetingAI.Target(self, ComponentHandle, TargetTag, bZAxisOnly, deltaTime)
    end
end

return TargetingAI 
