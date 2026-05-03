local WeaponDefs = require("WeaponDefs")

local MachineTurretWeapon = {}
MachineTurretWeapon.__index = MachineTurretWeapon

function MachineTurretWeapon.New(owner)
    local self = setmetatable({}, MachineTurretWeapon)
    self.Owner = owner
    self.Id = "MachineTurret"
    self.Level = 1
    self.Data = WeaponDefs.MachineTurret.Levels[self.Level]
    self.IsRunning = false
    return self
end

function MachineTurretWeapon:Start()
    self.IsRunning = true
    Log("MachineTurret started. count = " .. tostring(self.Data.TurretCount))
end

function MachineTurretWeapon:Stop()
    self.IsRunning = false
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
