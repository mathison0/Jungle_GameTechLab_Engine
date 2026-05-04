local Co = require("LuaCoroutine")
local GameplayPause = require("GameplayPause")
local WeaponDefs = require("WeaponDefs")

local HomingMissileWeapon = {}
HomingMissileWeapon.__index = HomingMissileWeapon

function HomingMissileWeapon.New(owner)
    local self = setmetatable({}, HomingMissileWeapon)
    self.Owner = owner
    self.Id = "HomingMissile"
    self.Level = 1
    self.Data = WeaponDefs.HomingMissile.Levels[self.Level]
    self.Coroutine = nil
    self.IsRunning = false
    return self
end

function HomingMissileWeapon:Start()
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

function HomingMissileWeapon:Stop()
    self.IsRunning = false

    if self.Owner ~= nil and self.Owner.StopCoroutine ~= nil and self.Coroutine ~= nil then
        self.Owner.StopCoroutine(self.Coroutine)
    end

    self.Coroutine = nil
end

function HomingMissileWeapon:FireLoop()
    while self.IsRunning do
        if not GameplayPause.IsPaused() then
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

            if target ~= nil and target:IsValid() and self.Owner.FireHomingMissile ~= nil then
                self.Owner.FireHomingMissile("HomingMissile", self.Data, 0, target)
            end
        end

        GameplayPause.Wait(self.Data.FireInterval)
    end
end

function HomingMissileWeapon:Upgrade()
    if self:IsMaxLevel() then
        Log("HomingMissile is max level")
        return false
    end

    self.Level = self.Level + 1
    self.Data = WeaponDefs.HomingMissile.Levels[self.Level]
    Log("HomingMissile upgraded. level = " .. tostring(self.Level))
    return true
end

function HomingMissileWeapon:IsMaxLevel()
    return self.Level >= WeaponDefs.HomingMissile.MaxLevel
end

return HomingMissileWeapon
