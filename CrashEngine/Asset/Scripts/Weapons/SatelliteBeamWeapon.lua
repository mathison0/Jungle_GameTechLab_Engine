local Co = require("LuaCoroutine")
local DamageSystem = require("Core.DamageSystem")
local WeaponDefs = require("WeaponDefs")

local SatelliteBeamWeapon = {}
SatelliteBeamWeapon.__index = SatelliteBeamWeapon

function SatelliteBeamWeapon.New(owner)
    local self = setmetatable({}, SatelliteBeamWeapon)
    self.Owner = owner
    self.Id = "SatelliteBeam"
    self.Level = 1
    self.Data = WeaponDefs.SatelliteBeam.Levels[self.Level]
    self.Coroutine = nil
    self.IsRunning = false
    return self
end

function SatelliteBeamWeapon:Start()
    if self.IsRunning then
        return
    end

    self.IsRunning = true

    if self.Owner ~= nil and self.Owner.StartCoroutine ~= nil then
        self.Coroutine = self.Owner.StartCoroutine(function()
            self:FireLoop()
        end)
    else
        Log("StartCoroutine is nil")
    end
end

function SatelliteBeamWeapon:Stop()
    self.IsRunning = false

    if self.Owner ~= nil and self.Owner.StopCoroutine ~= nil and self.Coroutine ~= nil then
        self.Owner.StopCoroutine(self.Coroutine)
    end

    self.Coroutine = nil
end

function SatelliteBeamWeapon:FireLoop()
    while self.IsRunning do
        local target = nil
        local ownerActor = nil

        if self.Owner ~= nil and self.Owner.GetActor ~= nil then
            ownerActor = self.Owner.GetActor()
        end

        if ownerActor ~= nil and ownerActor:IsValid() and self.Owner.QueryActorByTagClosest ~= nil then
            target = self.Owner.QueryActorByTagClosest(
                "Enemy",
                ownerActor:GetLocation(),
                self.Data.Range or 10000.0
            )
        end

        if target ~= nil and target:IsValid() then
            DamageSystem.ApplyDamage(target, self.Data.Damage or 10.0, ownerActor)

            if self.Owner.ApplyTargetDamage ~= nil then
                self.Owner.ApplyTargetDamage("SatelliteBeam", self.Data, target)
            end
        end

        Co.Wait(self.Data.FireInterval)
    end
end

function SatelliteBeamWeapon:Upgrade()
    if self:IsMaxLevel() then
        Log("SatelliteBeam is max level")
        return false
    end

    self.Level = self.Level + 1
    self.Data = WeaponDefs.SatelliteBeam.Levels[self.Level]
    Log("SatelliteBeam upgraded. level = " .. tostring(self.Level))
    return true
end

function SatelliteBeamWeapon:IsMaxLevel()
    return self.Level >= WeaponDefs.SatelliteBeam.MaxLevel
end

return SatelliteBeamWeapon
