local Co = require("LuaCoroutine")
local DamageSystem = require("Core.DamageSystem")
local WeaponDefs = require("WeaponDefs")

local MachineTurretWeapon = {}
MachineTurretWeapon.__index = MachineTurretWeapon

local function GetVecAxis(v, axisName, axisIndex)
    if v == nil then
        return nil
    end

    return v[axisName] or v[axisIndex]
end

local function IsValidTarget(target, origin, range)
    if target == nil or not target:IsValid() then
        return false
    end

    if target.IsVisible ~= nil and not target:IsVisible() then
        return false
    end

    if origin ~= nil and range ~= nil then
        local targetPos = target:GetLocation()
        if targetPos == nil then
            return false
        end

        local ox = GetVecAxis(origin, "x", 1)
        local oy = GetVecAxis(origin, "y", 2)
        local oz = GetVecAxis(origin, "z", 3)

        local tx = GetVecAxis(targetPos, "x", 1)
        local ty = GetVecAxis(targetPos, "y", 2)
        local tz = GetVecAxis(targetPos, "z", 3)

        if ox == nil or oy == nil or oz == nil or
           tx == nil or ty == nil or tz == nil then
            return false
        end

        local dx = tx - ox
        local dy = ty - oy
        local dz = tz - oz

        local distSq = dx * dx + dy * dy + dz * dz
        local rangeSq = range * range

        if distSq > rangeSq then
            return false
        end
    end

    return true
end

function MachineTurretWeapon.New(owner)
    local self = setmetatable({}, MachineTurretWeapon)

    self.Owner = owner
    self.Id = "MachineTurret"
    self.Def = WeaponDefs.MachineTurret
    self.Level = 1
    self.Data = self.Def.Levels[1]
    self.IsRunning = false
    self.TurretSlots = {}

    return self
end

function MachineTurretWeapon:Start()
    if self.IsRunning then
        return
    end

    self.IsRunning = true
    self:RebuildTurretSlots()
end

function MachineTurretWeapon:Stop()
    self.IsRunning = false

    for _, slot in pairs(self.TurretSlots) do
        slot.IsRunning = false

        if self.Owner ~= nil and self.Owner.StopCoroutine ~= nil then
            if slot.SearchCoroutine ~= nil then
                self.Owner.StopCoroutine(slot.SearchCoroutine)
            end

            if slot.AimCoroutine ~= nil then
                self.Owner.StopCoroutine(slot.AimCoroutine)
            end

            if slot.FireCoroutine ~= nil then
                self.Owner.StopCoroutine(slot.FireCoroutine)
            end
        end
    end
end

function MachineTurretWeapon:Upgrade()
    if self:IsMaxLevel() then
        Log("MachineTurret is max level")
        return false
    end

    self.Level = self.Level + 1
    self.Data = self.Def.Levels[self.Level]

    Log("[MachineTurret] upgraded to level " .. tostring(self.Level))
    return true
end

function MachineTurretWeapon:IsMaxLevel()
    return self.Level >= self.Def.MaxLevel
end

function MachineTurretWeapon:RebuildTurretSlots()
    for _, slot in pairs(self.TurretSlots) do
        slot.IsRunning = false

        if self.Owner ~= nil and self.Owner.StopCoroutine ~= nil then
            if slot.SearchCoroutine ~= nil then
                self.Owner.StopCoroutine(slot.SearchCoroutine)
            end

            if slot.AimCoroutine ~= nil then
                self.Owner.StopCoroutine(slot.AimCoroutine)
            end

            if slot.FireCoroutine ~= nil then
                self.Owner.StopCoroutine(slot.FireCoroutine)
            end
        end
    end

    self.TurretSlots = {}

    local count = self.Data.TurretCount or 1

    for i = 0, count - 1 do
        local slot = self:CreateTurretSlot(i)

        if slot ~= nil then
            self.TurretSlots[i] = slot
            self:StartTurretSlot(slot)
        end
    end

    Log("[MachineTurret] rebuilt slots. count = " .. tostring(count))
end

function MachineTurretWeapon:CreateTurretSlot(index)
    local visualName = "Visual_MachineTurret_" .. tostring(index)
    local muzzleName = "Muzzle_MachineTurret_" .. tostring(index)

    local visual = nil
    local muzzle = nil

    if self.Owner ~= nil and self.Owner.GetComponentByName ~= nil then
        visual = self.Owner.GetComponentByName("UStaticMeshComponent", visualName)
        muzzle = self.Owner.GetComponentByName("USceneComponent", muzzleName)
    end

    if visual == nil or not visual:IsValid() then
        Log("[MachineTurret] Visual not found: " .. visualName)
        return nil
    end

    return {
        Index = index,

        Visual = visual,
        Muzzle = muzzle,

        Target = nil,
        IsRunning = true,

        FireInterval = self.Data.FireInterval or 1.0,
        TargetRefreshInterval = self.Data.TargetRefreshInterval or 0.2,

        Range = self.Data.Range or 10000.0,
        Damage = self.Data.Damage or 1.0,

        SearchCoroutine = nil,
        AimCoroutine = nil,
        FireCoroutine = nil,
    }
end

function MachineTurretWeapon:GetSlotOrigin(slot)
    if slot == nil then
        return nil
    end

    if slot.Muzzle ~= nil and slot.Muzzle:IsValid() then
        return slot.Muzzle:GetWorldLocation()
    end

    if slot.Visual ~= nil and slot.Visual:IsValid() then
        return slot.Visual:GetWorldLocation()
    end

    if self.Owner ~= nil and self.Owner.GetActor ~= nil then
        local ownerActor = self.Owner.GetActor()
        if ownerActor ~= nil and ownerActor:IsValid() then
            return ownerActor:GetLocation()
        end
    end

    return nil
end

function MachineTurretWeapon:StartTurretSlot(slot)
    if self.Owner == nil or self.Owner.StartCoroutine == nil then
        Log("[MachineTurret] StartCoroutine is nil")
        return
    end

    slot.SearchCoroutine = self.Owner.StartCoroutine(function()
        self:TurretSearchLoop(slot)
    end)

    slot.AimCoroutine = self.Owner.StartCoroutine(function()
        self:TurretAimLoop(slot)
    end)

    slot.FireCoroutine = self.Owner.StartCoroutine(function()
        self:TurretFireLoop(slot)
    end)
end

function MachineTurretWeapon:TurretSearchLoop(slot)
    while self.IsRunning and slot.IsRunning do
        self:UpdateSlotTarget(slot)
        Co.Wait(slot.TargetRefreshInterval)
    end
end

function MachineTurretWeapon:TurretAimLoop(slot)
    while self.IsRunning and slot.IsRunning do
        Co.WaitNextFrame()
        self:UpdateSlotAim(slot)
    end
end

function MachineTurretWeapon:TurretFireLoop(slot)
    while self.IsRunning and slot.IsRunning do
        local rapidCount = self.Data.RapidCount or 1
        local rapidInterval = self.Data.RapidInterval or 0.05

        for i = 1, rapidCount do
            if not self.IsRunning or not slot.IsRunning then
                break
            end
            
            self:FireSlot(slot)

            if i < rapidCount then
                Co.Wait(rapidInterval)
            end
        end

        Co.Wait(slot.FireInterval)
    end
end

function MachineTurretWeapon:UpdateSlotTarget(slot)
    local origin = self:GetSlotOrigin(slot)

    if IsValidTarget(slot.Target, origin, slot.Range) then
        return
    end

    slot.Target = nil

    if origin == nil then
        return
    end

    if self.Owner ~= nil and self.Owner.QueryActorByTagClosest ~= nil then
        slot.Target = self.Owner.QueryActorByTagClosest("Enemy", origin, slot.Range)
    end
end

function MachineTurretWeapon:UpdateSlotAim(slot)
    local origin = self:GetSlotOrigin(slot)

    if not IsValidTarget(slot.Target, origin, slot.Range) then
        return
    end

    if slot.Visual == nil or not slot.Visual:IsValid() then
        return
    end

    local myPos = slot.Visual:GetWorldLocation()
    local targetPos = slot.Target:GetLocation()
    local z = myPos.z or myPos[3]

    if z ~= nil then
        targetPos[3] = z
        targetPos.z = z
    end

    slot.Visual:LookAt(targetPos)
end

function MachineTurretWeapon:FireSlot(slot)
    local origin = self:GetSlotOrigin(slot)

    if not IsValidTarget(slot.Target, origin, slot.Range) then
        slot.Target = nil
        return
    end

    local ownerActor = nil
    if self.Owner ~= nil and self.Owner.GetActor ~= nil then
        ownerActor = self.Owner.GetActor()
    end

    local success = DamageSystem.ApplyDamage(slot.Target, slot.Damage, ownerActor)

    if not success then
        Log("[MachineTurret] Damage failed. slot=" ..
            tostring(slot.Index) ..
            " damage=" ..
            tostring(slot.Damage))
    end

    if self.Owner ~= nil and self.Owner.NotifyInstantHit ~= nil then
        self.Owner.NotifyInstantHit("MachineTurret", self.Data, slot.Index, slot.Target)
    end
end

return MachineTurretWeapon