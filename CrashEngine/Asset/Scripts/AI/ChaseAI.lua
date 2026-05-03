local Co = require("LuaCoroutine")
local Vec = require("Core.Vector")

local ChaseAI = {
    properties = {
        MoveSpeed = { type = "float", default = 1.0, min = 0.1, max = 10.0, speed = 0.05},
        TargetSearchRadius = { type = "float", default = 10000.0, min = 0.0, max = 100000.0, speed = 100.0},
    }
}

function ChaseAI.ChaseTarget(self, TargetTag, ComponentHandle, DeltaTime)
        if ComponentHandle == nil or not ComponentHandle:IsValid() then
            return
        end

        local myPos = ComponentHandle:GetWorldLocation()
        if(self.target == nil or not self.target:IsValid()) then
            self.target = self.QueryActorByTagClosest(TargetTag, myPos, self.TargetSearchRadius or 10000.0)
        end

        if self.target ~= nil and self.target:IsValid() then
            local targetPos = self.target:GetLocation()
            local dir = Vec.DirectionTo(myPos, targetPos)
            local moveDelta = Vec.Mul(dir, (self.MoveSpeed or 1.0) * DeltaTime)
            ComponentHandle:SetWorldLocation(Vec.Add(myPos, moveDelta))
        end
end

function ChaseAI.ChaseTargetCoroutine(self, ComponentHandle, TargetTag)

    while true do
        local deltaTime = Co.WaitNextFrame()
        ChaseAI.ChaseTarget(self, ComponentHandle, TargetTag, deltaTime)
    end
end

return ChaseAI
