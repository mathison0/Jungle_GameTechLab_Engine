local WeaponInventory = {}
WeaponInventory.__index = WeaponInventory

local WeaponConstructors = {
    MainCannon = require("Weapons.MainCannonWeapon"),
    MachineTurret = require("Weapons.MachineTurretWeapon"),
    Aura = require("Weapons.AuraWeapon"),
    HomingMissile = require("Weapons.HomingMissileWeapon"),
    SatelliteBeam = require("Weapons.SatelliteBeamWeapon"),
}

local WeaponIds = {
    "MainCannon",
    "MachineTurret",
    "Aura",
    "HomingMissile",
    "SatelliteBeam",
}

function WeaponInventory.New(owner)
    local self = setmetatable({}, WeaponInventory)
    self.Owner = owner
    self.Weapons = {}
    Log("WeaponInventory created")
    return self
end

function WeaponInventory:HasWeapon(id)
    return self.Weapons[id] ~= nil
end

function WeaponInventory:EquipVisual(id, weapon)
    if self.Owner ~= nil and self.Owner.EquipWeaponVisual ~= nil and weapon ~= nil then
        self.Owner.EquipWeaponVisual(id, weapon.Level or 1)
    end
end

function WeaponInventory:AddWeapon(id)
    if self:HasWeapon(id) then
        return self:UpgradeWeapon(id)
    end

    local constructor = WeaponConstructors[id]
    if constructor == nil or constructor.New == nil then
        Log("Unknown weapon id = " .. tostring(id))
        return false
    end

    local weapon = constructor.New(self.Owner)
    self.Weapons[id] = weapon
    Log(id .. " added")
    self:EquipVisual(id, weapon)

    if weapon.Start ~= nil then
        weapon:Start()
    end

    return true
end

function WeaponInventory:UpgradeWeapon(id)
    local weapon = self.Weapons[id]
    if weapon == nil then
        return self:AddWeapon(id)
    end

    if weapon.Upgrade == nil then
        return false
    end

    local upgraded = weapon:Upgrade()
    if upgraded then
        self:EquipVisual(id, weapon)
    end

    return upgraded
end

function WeaponInventory:GetUpgradeableWeapons()
    local upgradeableWeapons = {}

    for _, id in ipairs(WeaponIds) do
        local weapon = self.Weapons[id]
        if weapon ~= nil and weapon.IsMaxLevel ~= nil and not weapon:IsMaxLevel() then
            table.insert(upgradeableWeapons, id)
        end
    end

    return upgradeableWeapons
end

function WeaponInventory:GetMissingWeapons()
    local missingWeapons = {}

    for _, id in ipairs(WeaponIds) do
        if not self:HasWeapon(id) then
            table.insert(missingWeapons, id)
        end
    end

    return missingWeapons
end

function WeaponInventory:GetEvolvableWeapons()
    local evolvableWeapons = {}

    for _, id in ipairs(WeaponIds) do
        local weapon = self.Weapons[id]
        if weapon ~= nil and weapon.CanEvolve ~= nil and weapon:CanEvolve() then
            table.insert(evolvableWeapons, id)
        end
    end

    return evolvableWeapons
end

function WeaponInventory:EvolveWeapon(id)
    local weapon = self.Weapons[id]
    if weapon == nil or weapon.Evolve == nil then
        return false
    end

    local evolved = weapon:Evolve()
    if evolved then
        self:EquipVisual(id, weapon)
    end

    return evolved
end

function WeaponInventory:OpenChest()
    local evolvableWeapons = self:GetEvolvableWeapons()
    if #evolvableWeapons > 0 then
        local id = evolvableWeapons[1]
        Log("OpenChest evolve = " .. tostring(id))
        return self:EvolveWeapon(id)
    end

    local upgradeableWeapons = self:GetUpgradeableWeapons()
    if #upgradeableWeapons > 0 then
        local id = upgradeableWeapons[math.random(#upgradeableWeapons)]
        Log("OpenChest upgrade = " .. tostring(id))
        return self:UpgradeWeapon(id)
    end

    Log("OpenChest failed")
    return false
end

return WeaponInventory
