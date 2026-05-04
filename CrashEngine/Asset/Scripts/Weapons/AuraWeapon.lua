local Co = require("LuaCoroutine")
local WeaponDefs = require("WeaponDefs")
local DamageSystem = require("Core.DamageSystem")


local AuraWeapon = {}
AuraWeapon.__index = AuraWeapon

function AuraWeapon.New(owner)
    local self = setmetatable({}, AuraWeapon)
    self.Owner = owner
    self.Id = "Aura"
    self.Level = 1
    self.Data = WeaponDefs.Aura.Levels[self.Level]
    self.Coroutine = nil
    self.IsRunning = false
    return self
end

function AuraWeapon:Start()
    if self.IsRunning then
        return
    end

    self.IsRunning = true

    if self.Owner ~= nil and self.Owner.StartCoroutine ~= nil then
        self.Coroutine = self.Owner.StartCoroutine(function()
            self:DamageLoop()
        end)
    else
        Log("StartCoroutine is nil")
    end
end

function AuraWeapon:Stop()
    self.IsRunning = false

    if self.Owner ~= nil and self.Owner.StopCoroutine ~= nil and self.Coroutine ~= nil then
        self.Owner.StopCoroutine(self.Coroutine)
    end

    self.Coroutine = nil
end

function AuraWeapon:DamageLoop()
    while self.IsRunning do
        local ownerActor = nil
        if self.Owner ~= nil and self.Owner.GetActor ~= nil then
            ownerActor = self.Owner.GetActor()
        end

        if ownerActor ~= nil and ownerActor:IsValid() and self.Owner.ApplyAreaDamage ~= nil then
            local enemies = self.Owner.QueryActorsByTagInRadius("Enemy", ownerActor:GetLocation(), self.Data.Radius)

            for _, enemy in pairs(enemies) do
                if enemy ~= nil and enemy:IsValid() then
                    DamageSystem.ApplyDamage(enemy, self.Data.Damage, ownerActor)
                end
            end

            self.Owner.ApplyAreaDamage("Aura", self.Data, ownerActor:GetLocation(), self.Data.Radius or 0.0)
        else
            Log("Aura tick. radius = " .. tostring(self.Data.Radius))
        end

        Co.Wait(self.Data.TickInterval)
    end
end

function AuraWeapon:Upgrade()
    if self:IsMaxLevel() then
        Log("Aura is max level")
        return false
    end

    self.Level = self.Level + 1
    self.Data = WeaponDefs.Aura.Levels[self.Level]
    Log("Aura upgraded. level = " .. tostring(self.Level) .. ", radius = " .. tostring(self.Data.Radius))
    return true
end

function AuraWeapon:IsMaxLevel()
    return self.Level >= WeaponDefs.Aura.MaxLevel
end

return AuraWeapon
