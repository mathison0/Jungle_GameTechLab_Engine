local Co = require("LuaCoroutine")
local Vec = require("Core.Vector")

local EnemyAI = {
    properties = {
        MoveSpeed = { type = "float", default = 1.0, min = 0.1, max = 10.0, speed = 0.05},
        TargetSearchRadius = { type = "float", default = 10000.0, min = 0.0, max = 100000.0, speed = 100.0},
    }
}

function EnemyAI.ChaseTarget(self, TargetTag)
    while true do
        local deltaTime = Co.WaitNextFrame()
        local myPos = self.GetLocation()
        local target = self.QueryActorByTagClosest(TargetTag, myPos, self.TargetSearchRadius or 10000.0)

        if target ~= nil and target:IsValid() then
            local targetPos = self.GetActorLocation(target)
            local dir = Vec.DirectionTo(myPos, targetPos)
            self.SetLocation(myPos + dir * (self.MoveSpeed or 1.0) * deltaTime)
        end

        
    end
end

return EnemyAI
