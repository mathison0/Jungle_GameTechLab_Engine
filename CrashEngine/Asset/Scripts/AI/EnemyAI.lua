local Co = require("LuaCoroutine")
local Vec = require("Core.Vector")

local EnemyAI = {
    properties = {
        MoveSpeed = { type = "float", default = 1.0, min = 0.1, max = 10.0, speed = 0.05},
        TargetSearchRadius = { type = "float", default = 10000.0, min = 0.0, max = 100000.0, speed = 100.0},
    }
}

function EnemyAI.ChaseTarget(self, TargetTag, DeltaTime)
        Log("[EnemyAI] ChaseTargetInternal")

        local actor = self.GetActor()
        if actor == nil or not actor:IsValid() then
            return
        end

        local myPos = actor:GetLocation()

        if(self.target == nil or not self.target:IsValid()) then
            Log("[EnemyAI] Search Target")
            self.target = self.QueryActorByTagClosest(TargetTag, myPos, self.TargetSearchRadius or 10000.0)
        end

        Log("[EnemyAI] Test Valid")
        if self.target ~= nil and self.target:IsValid() then
            Log("[EnemyAI] Chase Start")
            local targetPos = self.target:GetLocation()
            local dir = Vec.DirectionTo(myPos, targetPos)
            actor:SetLocation(myPos + dir * (self.MoveSpeed or 1.0) * DeltaTime)
        end
end

function EnemyAI.ChaseTargetCoroutine(self, TargetTag)
    Log("[EnemyAI] Start ChaseCoroutine")

    while true do
        local deltaTime = Co.WaitNextFrame()
        EnemyAI.ChaseTarget(self, TargetTag, deltaTime)
    end
end

return EnemyAI
