local Co = require("LuaCoroutine")
local WeaponDefs = require("WeaponDefs")

local MachineTurretWeapon = {}
MachineTurretWeapon.__index = MachineTurretWeapon

function MachineTurretWeapon.New(owner)
    local self = setmetatable({}, MachineTurretWeapon)
    self.Owner = owner
    self.Id = "MachineTurret"
    self.Level = 1
    self.Data = WeaponDefs.MachineTurret.Levels[self.Level]
    self.Coroutine = nil
    self.IsRunning = false
    return self
end

function MachineTurretWeapon:Start()
    if self.IsRunning then
        return
    end

    self.IsRunning = true
    Log("MachineTurret FireLoop started. count = " .. tostring(self.Data.TurretCount))

    if self.Owner ~= nil and self.Owner.StartCoroutine ~= nil then
        self.Coroutine = self.Owner.StartCoroutine(function()
            self:FireLoop()
        end)
    else
        Log("StartCoroutine is nil")
    end
end

function MachineTurretWeapon:Stop()
    self.IsRunning = false

    if self.Owner ~= nil and self.Owner.StopCoroutine ~= nil and self.Coroutine ~= nil then
        self.Owner.StopCoroutine(self.Coroutine)
    end

    self.Coroutine = nil
end

function MachineTurretWeapon:FireLoop()
    while self.IsRunning do
        local count = self.Data.TurretCount or 1

        for i = 0, count - 1 do
            if self.Owner ~= nil and self.Owner.FireLinearProjectile ~= nil then
                self.Owner.FireLinearProjectile("MachineTurret", self.Data, i)
            end
        end

        Co.Wait(self.Data.FireInterval)
    end
end

function MachineTurretWeapon:Upgrade()
    if self:IsMaxLevel() then
        Log("MachineTurret is max level")
        return false
    end

    self.Level = self.Level + 1
    self.Data = WeaponDefs.MachineTurret.Levels[self.Level]
    Log("MachineTurret upgraded. level = " .. tostring(self.Level) .. ", count = " .. tostring(self.Data.TurretCount))
    return true
end

function MachineTurretWeapon:IsMaxLevel()
    return self.Level >= WeaponDefs.MachineTurret.MaxLevel
end

return MachineTurretWeapon
