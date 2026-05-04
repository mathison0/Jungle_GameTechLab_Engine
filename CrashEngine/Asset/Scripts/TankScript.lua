---@class TankScript : ScriptComponent
local Script = {
    properties = {
        PickupExp = { type = "float", default = 1.0, min = 0.0, max = 1000.0, speed = 1.0 },
        PickupAttractSpeed = { type = "float", default = 20.0, min = 0.0, max = 1000.0, speed = 1.0 },
    }
}
local Vec = require("Core.Vector")
local Query = require("Query")
local Targeting = require("AI.TargetingAI")
local WeaponInventory = require("WeaponInventory")
local LevelSystem = require("LevelSystem")
local Vec = require("Core.Vector")

function Script:BeginPlay()
    Log("TankScript BeginPlay")

    Log("SearchHeadMainGun")
    self.HeadMainGun = self.GetComponentByName("UStaticMeshComponent", "HeadMainGun")
    if self.HeadMainGun == nil or not self.HeadMainGun:IsValid() then
        Log("Invalid HeadMainGun")
    else
        Log("StarCoroutineSearch")
        self.Search = self.StartCoroutine(function()
            Query.SearchActorClosestByTagCoroutine(self, self.HeadMainGun, "Enemy")
        end)

        Log("StarCoroutineTargeting")
        self.Targeting = self.StartCoroutine(function()
            Targeting.TargetingCoroutine(self, self.HeadMainGun, true)
        end)
    end

    self.WeaponInventory = WeaponInventory.New(self)
    self.LevelSystem = LevelSystem.New(self, self.WeaponInventory)

    self.WeaponInventory:AddWeapon("MainCannon")
    self.RootCollider = self.GetRootComponent()
    if self.RootCollider == nil or not self.RootCollider:IsValid() then
        Log("Invalid RootCollider")
    end

    self.PickupSensor = self.GetComponentByName("UCircleCollider2DComponent", "PickupSensor")
    if self.PickupSensor == nil or not self.PickupSensor:IsValid() then
        Log("Invalid PickupSensor")
    end
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

function Script:AttractPickup(pickup, deltaTime)
    if pickup == nil or not pickup:IsValid() then
        return
    end

    local owner = self.GetActor()
    if owner == nil or not owner:IsValid() then
        return
    end

    local pickupPos = pickup:GetLocation()
    local targetPos = owner:GetLocation()
    local toTarget = Vec.Sub(targetPos, pickupPos)
    local distance = Vec.Length(toTarget)
    if distance <= 0.0001 then
        return
    end

    local step = math.min(distance, (self.PickupAttractSpeed or 20.0) * deltaTime)
    pickup:SetLocation(Vec.Add(pickupPos, Vec.Mul(toTarget, step / distance)))
end

function Script:CollectPickup(pickup, poolManager)
    self:AddExp(self.PickupExp or 1)

    if poolManager ~= nil and poolManager:IsValid() then
        poolManager:Release(pickup)
    end
    pickup:SetVisible(false)
end

function Script:UpdatePickups(deltaTime)
    if self.PickupSensor == nil or not self.PickupSensor:IsValid() then
        return
    end
    if self.RootCollider == nil or not self.RootCollider:IsValid() then
        return
    end

    local world = self.GetWorld()
    if world == nil or not world:IsValid() then
        return
    end

    local pickups = world:GetActorsByTag("Pickup")
    if pickups == nil then
        return
    end

    local poolManager = GetActorPoolManager()
    for _, pickup in ipairs(pickups) do
        if pickup:IsValid() and pickup:IsVisible() then
            if self.PickupSensor:IsOverlappingActor(pickup) then
                self:AttractPickup(pickup, deltaTime)
            end

            if self.RootCollider:IsOverlappingActor(pickup) then
                self:CollectPickup(pickup, poolManager)
            end
        end
    end
end

function Script:Tick(deltaTime)
    self:UpdatePickups(deltaTime)
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
