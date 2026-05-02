local Co = require("LuaCoroutine")
local Vec = require("Core.Vector")
local Handle = require("Core.Handle")

local TurretHead = {}

function TurretHead.RotateToward(self, HeadName, TargetTag)
    local HeadHandle = self.GetComponentHandle("", HeadName)

    while true do
        local DeltaTime = Co.WaitNextFrame()

        if not Handle.IsValid(HeadHandle) then
            HeadHandle = self.GetComponentHandle("", HeadName)
        end

        if Handle.IsValid(HeadHandle) then
            local HeadLocation = self.GetSceneComponentLocation(HeadHandle)
            local SearchRadius = self.TargetSearchRadius or 10000.0
            local TargetHandle = self.QueryActorByTagClosest(TargetTag, HeadLocation, SearchRadius)

            if Handle.IsValid(TargetHandle) then
                local TargetLocation = self.GetActorLocation(TargetHandle)
                local Direction = Vec.DirectionTo(HeadLocation, TargetLocation)
                local RotateSpeed = self.RotateSpeed or 90.0
                self.RotateTurretHead(HeadHandle, Direction, RotateSpeed * DeltaTime)
            end
        end
    end
end

return TurretHead
