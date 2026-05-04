--[[
    GameManager.lua
    순서 독립적으로 초기화되는 전역 매니저.
]]

local WeaponInventory = require("WeaponInventory")
local LevelSystem = require("LevelSystem")

local GameManager = {
    -- Config (From Component)
    GameTimeLimit = 600.0,
    
    -- State
    TimeRemaining = 600.0,
    KillCount = 0,
    IsGameOver = false,
    IsPaused = false,
    
    -- Flags
    SessionPrepared = false,
    PlayerRegistered = false,
    Initialized = false,

    -- Player Data
    PlayerScript = nil,
    WeaponInventory = nil,
    LevelSystem = nil,
    Stats = {}
}

-- [내부 리셋 함수]
function GameManager._ResetState()
    -- 스탯 리셋
    GameManager.Stats = {
        MaxHP = 100.0, CurrentHP = 100.0,
        MoveSpeedMult = 1.0, ExpMult = 1.0,
        PickupRangeMult = 1.0, DamageMult = 1.0,
    }
    
    -- 상태 및 플래그 완전 초기화 (세션 영속성 방지)
    GameManager.KillCount = 0
    GameManager.IsGameOver = false
    GameManager.IsPaused = false
    GameManager.Initialized = false
    
    GameManager.SessionPrepared = false
    GameManager.PlayerRegistered = false
    
    GameManager.PlayerScript = nil
    GameManager.WeaponInventory = nil
    GameManager.LevelSystem = nil
    
    Log("[GameManager] Session State Fully Reset.")
end

-- [1단계: 세션 설정 준비]
function GameManager.PrepareSession(gameTimeLimit)
    GameManager._ResetState() -- 새로운 세션 시작 시 무조건 리셋
    
    GameManager.GameTimeLimit = gameTimeLimit or 600.0
    GameManager.TimeRemaining = GameManager.GameTimeLimit
    GameManager.SessionPrepared = true
    
    Log("[GameManager] Session Prepared (TimeLimit: " .. tostring(GameManager.GameTimeLimit) .. ")")
    GameManager._CheckAndStart()
end

-- [2단계: 플레이어 등록]
function GameManager.RegisterPlayer(playerScript)
    GameManager.PlayerScript = playerScript
    
    -- 서브시스템 생성 (테이블 참조 유지)
    GameManager.WeaponInventory = WeaponInventory.New(playerScript)
    GameManager.LevelSystem = LevelSystem.New(playerScript, GameManager.WeaponInventory)
    
    GameManager.PlayerRegistered = true
    Log("[GameManager] Player Registered.")
    GameManager._CheckAndStart()
end

-- [3단계: 조건 충족 시 시작]
function GameManager._CheckAndStart()
    if GameManager.SessionPrepared and GameManager.PlayerRegistered and not GameManager.Initialized then
        GameManager.Initialized = true
        GameManager.OnGameStart()
    end
end

function GameManager.OnGameStart()
    if GameManager.WeaponInventory then
        GameManager.WeaponInventory:AddWeapon("Aura")
    end
    Log("[GameManager] --- GAME START ---")
end

function GameManager.Tick(deltaTime)
    if not GameManager.Initialized or GameManager.IsGameOver or GameManager.IsPaused then
        return
    end

    if type(deltaTime) ~= "number" then return end

    GameManager.TimeRemaining = math.max(0, GameManager.TimeRemaining - deltaTime)
    if GameManager.TimeRemaining <= 0 then
        GameManager.OnGameOver("Time Up!")
    end
end

-- ... (나머지 데미지, 스탯 처리 함수들은 동일) ...

function GameManager.PlayerGetDamage(damage)
    if GameManager.IsGameOver then return end
    GameManager.Stats.CurrentHP = math.max(0, GameManager.Stats.CurrentHP - damage)
    if GameManager.Stats.CurrentHP <= 0 then GameManager.OnGameOver("Player Died") end
end

function GameManager.OnPickupExp(baseAmount)
    local totalExp = baseAmount * (GameManager.Stats.ExpMult or 1.0)
    if GameManager.LevelSystem then GameManager.LevelSystem:AddExp(totalExp) end
end

function GameManager.OnPickupChest()
    if GameManager.WeaponInventory then return GameManager.WeaponInventory:OpenChest() end
    return false
end

function GameManager.OnGameOver(reason)
    if GameManager.IsGameOver then return end
    GameManager.IsGameOver = true
    Log("[GameManager] GAME OVER: " .. tostring(reason))
end

return GameManager
