local Co = require("LuaCoroutine")
local DamageSystem = require("Core.DamageSystem")
local WeaponDefs = require("WeaponDefs")

local MachineTurretWeapon = {}
MachineTurretWeapon.__index = MachineTurretWeapon

local function IsValidTarget(target)
    if target == nil or not target:IsValid() then
        return false
    end

    if target.IsVisible ~= nil and not target:IsVisible() then
        return false
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

        if self.Owner ~= nil and self.Owner.StopCoroutine ~= nil and slot.Coroutine ~= nil then
            self.Owner.StopCoroutine(slot.Coroutine)
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

    if self.Owner ~= nil and self.Owner.EquipWeaponVisual ~= nil then
        self.Owner.EquipWeaponVisual(self.Id, self.Level)
    end

    self:RebuildTurretSlots()

    Log("[MachineTurret] upgraded to level " .. tostring(self.Level))
    return true
end

function MachineTurretWeapon:IsMaxLevel()
    return self.Level >= self.Def.MaxLevel
end

function MachineTurretWeapon:RebuildTurretSlots()
    for _, slot in pairs(self.TurretSlots) do
        slot.IsRunning = false

        if self.Owner ~= nil and self.Owner.StopCoroutine ~= nil and slot.Coroutine ~= nil then
            self.Owner.StopCoroutine(slot.Coroutine)
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

        FireTimer = 0.0,
        TargetTimer = 0.0,
        Coroutine = nil,
    }
end

function MachineTurretWeapon:StartTurretSlot(slot)
    if self.Owner == nil or self.Owner.StartCoroutine == nil then
        Log("[MachineTurret] StartCoroutine is nil")
        return
    end

    slot.Coroutine = self.Owner.StartCoroutine(function()
        self:TurretSlotLoop(slot)
    end)
end

function MachineTurretWeapon:TurretSlotLoop(slot)
    while self.IsRunning and slot.IsRunning do
        local dt = Co.WaitNextFrame() or 0.0

        slot.FireTimer = slot.FireTimer + dt
        slot.TargetTimer = slot.TargetTimer + dt

        if slot.TargetTimer >= slot.TargetRefreshInterval then
            slot.TargetTimer = 0.0
            self:UpdateSlotTarget(slot)
        end

        self:UpdateSlotAim(slot)

        if slot.FireTimer >= slot.FireInterval then
            slot.FireTimer = 0.0
            self:FireSlot(slot)
        end
    end
end

function MachineTurretWeapon:UpdateSlotTarget(slot)
    if IsValidTarget(slot.Target) then
        return
    end

    slot.Target = nil

    local origin = nil

    if slot.Visual ~= nil and slot.Visual:IsValid() then
        origin = slot.Visual:GetWorldLocation()
    elseif self.Owner ~= nil and self.Owner.GetActor ~= nil then
        local ownerActor = self.Owner.GetActor()
        if ownerActor ~= nil and ownerActor:IsValid() then
            origin = ownerActor:GetLocation()
        end
    end

    if origin == nil then
        return
    end

    if self.Owner ~= nil and self.Owner.QueryActorByTagClosest ~= nil then
        slot.Target = self.Owner.QueryActorByTagClosest("Enemy", origin, slot.Range)
    end
end

function MachineTurretWeapon:UpdateSlotAim(slot)
    if not IsValidTarget(slot.Target) then
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
    if not IsValidTarget(slot.Target) then
        return
    end

    local ownerActor = nil
    if self.Owner ~= nil and self.Owner.GetActor ~= nil then
        ownerActor = self.Owner.GetActor()
    end

    local success = DamageSystem.ApplyDamage(slot.Target, slot.Damage, ownerActor)

    if success then
        Log("[MachineTurret] Instant hit. slot=" ..
            tostring(slot.Index) ..
            " damage=" ..
            tostring(slot.Damage))
    end

    if self.Owner ~= nil and self.Owner.NotifyInstantHit ~= nil then
        self.Owner.NotifyInstantHit("MachineTurret", self.Data, slot.Index, slot.Target)
    end
end

return MachineTurretWeapon
