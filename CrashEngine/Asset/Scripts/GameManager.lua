--[[

    Lua 스크립트 내에서 전역으로 접근할 수 있는 GameManager.

]] 

local WeaponInventory = require("WeaponInventory")
local LevelSystem = require("LevelSystem")

---@class GameManager
local GameManager = {
    -- State
    KillCount = 0,
    GameTimeLimit = 600.0,
    TimeRemaining = 600.0,
    IsGameOver = false,
    IsPaused = false,

    -- Player Stats
    Stats = {
        MaxHP = 100.0,
        CurrentHP = 100.0,
        MoveSpeedMult = 1.0,
        ExpMult = 1.0,
        PickupRangeMult = 1.0,
        DamageMult = 1.0,
    },

    -- Subsystems
    WeaponInventory = nil,
    LevelSystem = nil,
    PlayerScript = nil,
    Initialized = false,
}

function GameManager.Init(playerScript, gameTimeLimit)
    if GameManager.Initialized then 
        Log("[GameManager] Already Initialized. Skipping...")
        return 
    end

    GameManager.PlayerScript = playerScript
    GameManager.GameTimeLimit = gameTimeLimit or 600.0
    GameManager.TimeRemaining = GameManager.GameTimeLimit
    
    -- WeaponInventory and LevelSystem ownership
    GameManager.WeaponInventory = WeaponInventory.New(playerScript)
    GameManager.LevelSystem = LevelSystem.New(playerScript, GameManager.WeaponInventory)
    
    GameManager.Initialized = true
    Log("[GameManager] Initialized with TimeLimit: " .. tostring(GameManager.GameTimeLimit))
end

function GameManager.OnGameStart()
    GameManager.KillCount = 0
    GameManager.TimeRemaining = GameManager.GameTimeLimit
    GameManager.Stats.CurrentHP = GameManager.Stats.MaxHP
    GameManager.IsGameOver = false
    GameManager.IsPaused = false
    
    -- 기본 무기 지급
    if GameManager.WeaponInventory then
        GameManager.WeaponInventory:AddWeapon("MainCannon")
    end
    
    Log("[GameManager] Game Started")
end

function GameManager.Tick(deltaTime)
    if GameManager.IsGameOver or GameManager.IsPaused then
        return
    end

    -- 시간 감소
    GameManager.TimeRemaining = math.max(0, GameManager.TimeRemaining - deltaTime)
    
    if GameManager.TimeRemaining <= 0 then
        GameManager.OnGameOver("Time Up!")
    end
end

-- [데미지 처리]
function GameManager.PlayerGetDamage(damage)
    if GameManager.IsGameOver then return end
    
    GameManager.Stats.CurrentHP = math.max(0, GameManager.Stats.CurrentHP - damage)
    Log("[GameManager] Player took " .. tostring(damage) .. " damage. HP: " .. tostring(GameManager.Stats.CurrentHP))
    
    if GameManager.Stats.CurrentHP <= 0 then
        GameManager.OnGameOver("Player Died")
    end
end

-- [HP 회복]
function GameManager.PlayerHeal(amount)
    if GameManager.IsGameOver then return end
    GameManager.Stats.CurrentHP = math.min(GameManager.Stats.MaxHP, GameManager.Stats.CurrentHP + amount)
    Log("[GameManager] Player healed " .. tostring(amount) .. ". HP: " .. tostring(GameManager.Stats.CurrentHP))
end

-- [처치 기록]
function GameManager.AddKill()
    GameManager.KillCount = GameManager.KillCount + 1
end

-- [경험치/아이템 획득 통합 핸들러]
function GameManager.OnPickupExp(baseAmount)
    local totalExp = baseAmount * GameManager.Stats.ExpMult
    if GameManager.LevelSystem then
        GameManager.LevelSystem:AddExp(totalExp)
    end
end

function GameManager.OnPickupChest()
    if GameManager.WeaponInventory then
        return GameManager.WeaponInventory:OpenChest()
    end
    return false
end

-- [유틸리티]
function GameManager.AddExp(amount) -- 하위 호환성 유지
    GameManager.OnPickupExp(amount)
end

function GameManager.SetPaused(paused)
    GameManager.IsPaused = paused
    Log("[GameManager] Paused: " .. tostring(paused))
end

function GameManager.OnGameOver(reason)
    if GameManager.IsGameOver then return end
    
    GameManager.IsGameOver = true
    Log("[GameManager] Game Over! Reason: " .. tostring(reason))
    -- 추가적인 게임오버 연출이나 UI 호출 로직이 여기 들어갈 수 있음
end

function GameManager.RestartGame()
    Log("[GameManager] Restarting Game...")
    GameManager.OnGameStart()
end

-- [스탯 관리]
function GameManager.UpdateStat(statName, value)
    if GameManager.Stats[statName] ~= nil then
        GameManager.Stats[statName] = GameManager.Stats[statName] + value
        Log("[GameManager] Stat '" .. statName .. "' updated: " .. tostring(GameManager.Stats[statName]))
    else
        Log("[GameManager] Stat '" .. statName .. "' not found.")
    end
end

function GameManager.SetStat(statName, value)
    if GameManager.Stats[statName] ~= nil then
        GameManager.Stats[statName] = value
        Log("[GameManager] Stat '" .. statName .. "' set to: " .. tostring(value))
    end
end

return GameManager
