local Co = require("LuaCoroutine")
local Vec = require("Core.Vector")

local Query = {}


function Query.SearchActorClosestByTag(self, ComponentHandle, TargetTag)
    self.target = self.QueryActorByTagClosest(TargetTag, ComponentHandle:GetWorldLocation(), self.TargetSearchRadius or 10000.0)
end

function Query.SearchActorClosestByTagCoroutine(self, ComponentHandle, TargetTag)
    while true do
        Query.SearchActorClosestByTag(self, ComponentHandle, TargetTag)
        Co.Wait(0.2)
    end
end

return Query