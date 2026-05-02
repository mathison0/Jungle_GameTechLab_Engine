local Co = require("LuaCoroutine")

local EnemyAI = {
    properties = {
        MoveSpeed = { type = "float", default = 1.0, min = 0.1, max = 10.0, speed = 0.05},
    }
}

function EnemyAI.ChaseTarget(self, TargetTag)
    while true do
        local deltaTime = Co.WaitNextFrame()
        local target = self:find_actor_by_tag(TargetTag)

        if target ~= nil then
            local targetPos = target:get_location()
            local myPos = self:get_location()
            local locationDelta = targetPos - self:get_location()
            local dir = locationDelta.normalize();
            self:set_Location(myPos + dir * MoveSpeed * deltaTime)
        end

        
    end
end

return EnemyAI