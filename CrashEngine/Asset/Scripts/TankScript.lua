---@class TankScript : ScriptComponent
local Script = {}
local Query = require("Query")
local Targeting = require("AI.TargetingAI")
local WeaponInventory = require("WeaponInventory")
local LevelSystem = require("LevelSystem")

function Script:BeginPlay()
    Log("TankScript BeginPlay")

    Log("SearchHeadMainGun")
    self.HeadMainGun = self.GetComponentByName("UStaticMeshComponent", "HeadMainGun")
    if self.HeadMainGun == nil or not self.HeadMainGun:IsValid() then
        Log("Invalid HeadMainGun")
    else
        Log("StarCoroutineSearch")
        self.Search = self.StartCoroutine(function()
            Query.SearchActorClosestByTagCoroutine(self, self.HeadMainGun, "TestEnemy")
        end)

        Log("StarCoroutineTargeting")
        self.Targeting = self.StartCoroutine(function()
            Targeting.TargetingCoroutine(self, self.HeadMainGun, true)
        end)
    end

    self.WeaponInventory = WeaponInventory.New(self)
    self.LevelSystem = LevelSystem.New(self, self.WeaponInventory)

    self.WeaponInventory:AddWeapon("MainCannon")

    -- Test only. In actual gameplay, enemy kills should call AddExp.
    self.LevelSystem:AddExp(30)
end

function Script:AddExp(amount)
    if self.LevelSystem ~= nil then
        self.LevelSystem:AddExp(amount)
    end
end

function Script:OpenChest()
    if self.WeaponInventory ~= nil then
        return self.WeaponInventory:OpenChest()
    end

    return false
end

function Script:Tick(deltaTime)

end

function Script:EndPlay()
    if self.WeaponInventory ~= nil then
        for _, weapon in pairs(self.WeaponInventory.Weapons) do
            if weapon.Stop ~= nil then
                weapon:Stop()
            end
        end
    end

    self.StopAllCoroutines()
end

return Script
