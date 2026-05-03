---@class TankScript : ScriptComponent
local Script = {}
local Query = require("Query")
local Targeting = require("AI.TargetingAI")
local Co = require("LuaCoroutine")

function Script:FireMainGun()
    while true do
        self.FireHeadMainGun()
        Co.Wait(0.5)
    end
end

function Script:BeginPlay()
    Log("SearchHeadMainGun")
    self.HeadMainGun = self.GetComponentByName("UStaticMeshComponent", "HeadMainGun");
    if self.HeadMainGun == nil or not self.HeadMainGun:IsValid() then
        Log("Invalid HeadMainGun")
        return
    end

    Log("StarCoroutineSearch")
    self.Search = self.StartCoroutine(function()
        Query.SearchActorClosestByTagCoroutine(self, self.HeadMainGun, "TestEnemy")
    end)
    Log("StarCoroutineTargeting")
    self.Targeting = self.StartCoroutine(function()
        Targeting.TargetingCoroutine(self, self.HeadMainGun, true)
    end)
    Log("StarCoroutineHeadMainGunFire")
    self.HeadMainGunFire = self.StartCoroutine(function()
        Script.FireMainGun(self)
    end)
end

function Script:Tick(deltaTime)

end

function Script:EndPlay()
    self.StopAllCoroutines()
end

return Script
