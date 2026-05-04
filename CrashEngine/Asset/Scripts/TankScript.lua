---@class TankScript : ScriptComponent
local Script = {
    properties = {
        MoveSpeed = { type = "float", default = 1.0 },
        RotateSpeed = { type = "float", default = 1.0 },
        BaseFriction = { type = "float", default = 0.1 },
        DriftFactor = { type = "float", default = 0.3 },
        DriftSmoothness = { type = "float", default = 0.1 },
        PickupExp = { type = "float", default = 1.0, min = 0.0, max = 1000.0, speed = 1.0 },
        PickupAttractSpeed = { type = "float", default = 20.0, min = 0.0, max = 1000.0, speed = 1.0 },
    }
}
local Vec = require("Core.Vector")
local WeaponInventory = require("WeaponInventory")
local LevelSystem = require("LevelSystem")

local KEY_SHIFT = 0x10

function Script:BeginPlay()
    self.Velocity = Vec.Zero()
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

function Script:UpdateMovement(deltaTime)
    local actor = self.GetActor()
    if actor == nil or not actor:IsValid() then
        return
    end

    local inputH = Input.GetAxis("Horizontal")
    local inputV = Input.GetAxis("Vertical")
    local driftButton = Input.GetKey(KEY_SHIFT)

    local rotateSpeedSign = 1.0
    if inputV < 0.0 then
        rotateSpeedSign = -1.0
    end

    local driftSmoothness = 0.8
    if driftButton then
        driftSmoothness = self.DriftSmoothness or 0.1
    end

    local currentRotateSpeed = self.RotateSpeed or 1.0
    if driftButton then
        currentRotateSpeed = currentRotateSpeed * 1.5
    end

    local trackWidth = 0.8
    local leftTrackSpeed = (self.MoveSpeed or 1.0) * inputV + currentRotateSpeed * inputH * rotateSpeedSign
    local rightTrackSpeed = (self.MoveSpeed or 1.0) * inputV - currentRotateSpeed * inputH * rotateSpeedSign

    local targetForwardSpeed = (leftTrackSpeed + rightTrackSpeed) * 0.5
    local targetAngularSpeed = (leftTrackSpeed - rightTrackSpeed) / trackWidth

    local forward = actor:GetForward()
    local intendedVelocity = Vec.Mul(forward, targetForwardSpeed)

    if self.Velocity == nil then
        self.Velocity = Vec.Zero()
    end

    local lerpAlpha = math.min(1.0, driftSmoothness * deltaTime * 10.0)
    self.Velocity = Vec.Lerp(self.Velocity, intendedVelocity, lerpAlpha)

    local location = actor:GetLocation()
    local newLocation = Vec.Add(location, Vec.Mul(self.Velocity, deltaTime))
    actor:SetLocation(newLocation)

    local rotation = actor:GetRotation()
    rotation.z = rotation.z + targetAngularSpeed * deltaTime
    actor:SetRotation(rotation)
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
    self:UpdateMovement(deltaTime)
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
