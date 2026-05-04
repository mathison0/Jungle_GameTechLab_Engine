local Co = require("LuaCoroutine")
local WeaponDefs = require("WeaponDefs")

local MainCannonWeapon = {}
MainCannonWeapon.__index = MainCannonWeapon

function MainCannonWeapon.New(owner)
    local self = setmetatable({}, MainCannonWeapon)
    self.Owner = owner
    self.Id = "MainCannon"
    self.Level = 1
    self.Data = WeaponDefs.MainCannon.Levels[self.Level]
    self.Coroutine = nil
    self.IsRunning = false
    return self
end

function MainCannonWeapon:Start()
    if self.IsRunning then
        return
    end

    self.IsRunning = true
    Log("MainCannon FireLoop started")

    if self.Owner ~= nil and self.Owner.StartCoroutine ~= nil then
        self.Coroutine = self.Owner.StartCoroutine(function()
            self:FireLoop()
        end)
    else
        Log("StartCoroutine is nil")
    end
end

function MainCannonWeapon:Stop()
    self.IsRunning = false

    if self.Owner ~= nil and self.Owner.StopCoroutine ~= nil and self.Coroutine ~= nil then
        self.Owner.StopCoroutine(self.Coroutine)
    end

    self.Coroutine = nil
end

function MainCannonWeapon:FireLoop()
    while self.IsRunning do
        if self.Owner ~= nil and self.Owner.FireLinearProjectile ~= nil then
            self.Owner.FireLinearProjectile("MainCannon", self.Data, 0)
        elseif self.Owner ~= nil and self.Owner.FireHeadMainGun ~= nil then
            self.Owner.FireHeadMainGun(self.Data)
        else
            Log("FireLinearProjectile / FireHeadMainGun is nil")
        end

        Co.Wait(self.Data.FireInterval)
    end
end

function MainCannonWeapon:Upgrade()
    if self.Level >= WeaponDefs.MainCannon.MaxNormalLevel then
        Log("MainCannon is max normal level")
        return false
    end

    self.Level = self.Level + 1
    self.Data = WeaponDefs.MainCannon.Levels[self.Level]
    Log("MainCannon upgraded. level = " .. tostring(self.Level) .. ", interval = " .. tostring(self.Data.FireInterval))
    return true
end

function MainCannonWeapon:IsMaxLevel()
    return self.Level >= WeaponDefs.MainCannon.MaxNormalLevel
end

function MainCannonWeapon:CanEvolve()
    return self.Level == WeaponDefs.MainCannon.MaxNormalLevel
end

function MainCannonWeapon:Evolve()
    if not self:CanEvolve() then
        return false
    end

    self.Level = 3
    self.Data = WeaponDefs.MainCannon.Levels[self.Level]
    Log("MainCannon evolved. level = " .. tostring(self.Level) .. ", interval = " .. tostring(self.Data.FireInterval))
    return true
end

return MainCannonWeapon
