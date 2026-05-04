local Co = require("LuaCoroutine")
local WeaponDefs = require("WeaponDefs")

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
        Log("Aura tick. radius = " .. tostring(self.Data.Radius))
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
