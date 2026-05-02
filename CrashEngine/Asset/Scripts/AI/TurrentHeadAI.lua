local Co = require("LuaCoroutine")

local TurretHead = {
    properties = {
        RotateSpeed = { type = "float", default = 90.0, min = 0.0, max = 1080, speed = 10},
        SearchInterval = { type = "float", default = 0.02, min = 0.0, max = 100, speed = 1},
    }
}

function TurretHead.SearchTarget(self)
    while true do
        if self.target ~= nil and self:is_actor_valid(self.target) then
            break
        end

        self.target = self:query_actor_by_tag_closest(TargetTag, position, radius)

        if self.target == nil then
            Co.Wait(SearchInterval)   
        end
    end
end

function TurretHead.RotateToward(self, TargetTag)
    self.SearchCoroutine = self:start_coroutine(Turret.SearchTarget)
    while self.target ~= nil do
        local deltaTime = Co.WaitNextFrame()

        if self.target == nil or not self:is_actor_valid(self.target) then
            self.target = nil
        else
            local myPos = self:get_location()
            local targetPos = self.target:get_location()

            local toTarget = targetPos - myPos

            if toTarget:length_squared() > 0.0001 then
                local targetDir = toTarget:normalized()

                local rotateSpeed = GetProp(self, "RotateSpeed")
                local maxDegree = rotateSpeed * deltaTime

                self:rotate_towards_direction(targetDir, maxDegree)
            end
        end
    end
end

return TurrentHead