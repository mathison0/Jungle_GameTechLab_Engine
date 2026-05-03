local LevelSystem = {}
LevelSystem.__index = LevelSystem

function LevelSystem.New(owner, weaponInventory)
    local self = setmetatable({}, LevelSystem)
    self.Owner = owner
    self.WeaponInventory = weaponInventory
    self.Level = 1
    self.Exp = 0
    self.RequiredExp = 10
    self.PendingLevelUps = 0
    Log("LevelSystem created")
    return self
end

function LevelSystem:AddExp(amount)
    if amount == nil or amount <= 0 then
        return
    end

    self.Exp = self.Exp + amount
    Log("LevelSystem AddExp " .. tostring(amount))

    while self.Exp >= self.RequiredExp do
        self.Exp = self.Exp - self.RequiredExp
        self.Level = self.Level + 1
        self.PendingLevelUps = self.PendingLevelUps + 1
        self.RequiredExp = self:CalcRequiredExp(self.Level)
        Log("Level up. level = " .. tostring(self.Level) .. ", pending = " .. tostring(self.PendingLevelUps))
    end

    if self.PendingLevelUps > 0 then
        self:RequestLevelUpSelection()
    end
end

function LevelSystem:CalcRequiredExp(level)
    return 10 + level * 5
end

function LevelSystem:RequestLevelUpSelection()
    while self.PendingLevelUps > 0 do
        local options = self:GenerateOptions(3)
        if #options <= 0 then
            Log("No level up options")
            return false
        end

        self.PendingLevelUps = self.PendingLevelUps - 1
        self:SelectUpgrade(options[1])
    end

    return true
end

function LevelSystem:GenerateOptions(count)
    local options = {}

    if self.WeaponInventory == nil then
        return options
    end

    local upgradeableWeapons = self.WeaponInventory:GetUpgradeableWeapons()
    for _, id in ipairs(upgradeableWeapons) do
        if #options >= count then
            break
        end

        table.insert(options, {
            Type = "UpgradeWeapon",
            WeaponId = id,
        })
    end

    local missingWeapons = self.WeaponInventory:GetMissingWeapons()
    for _, id in ipairs(missingWeapons) do
        if #options >= count then
            break
        end

        table.insert(options, {
            Type = "AddWeapon",
            WeaponId = id,
        })
    end

    Log("GenerateOptions count = " .. tostring(#options))
    return options
end

function LevelSystem:SelectUpgrade(option)
    if option == nil or self.WeaponInventory == nil then
        return false
    end

    Log("SelectUpgrade type = " .. tostring(option.Type) .. ", weapon = " .. tostring(option.WeaponId))

    if option.Type == "AddWeapon" then
        return self.WeaponInventory:AddWeapon(option.WeaponId)
    end

    if option.Type == "UpgradeWeapon" then
        return self.WeaponInventory:UpgradeWeapon(option.WeaponId)
    end

    return false
end

return LevelSystem
