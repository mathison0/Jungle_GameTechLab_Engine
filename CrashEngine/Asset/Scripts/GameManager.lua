local WeaponInventory = require("WeaponInventory")
local LevelSystem = require("LevelSystem")

local GameManager = {
    GameTimeLimit = 600.0,

    TimeRemaining = 600.0,
    KillCount = 0,
    IsGameOver = false,
    IsPaused = false,

    SessionPrepared = false,
    PlayerRegistered = false,
    Initialized = false,

    PlayerScript = nil,
    WeaponInventory = nil,
    LevelSystem = nil,
    World = nil,
    LevelUpUI = nil,
    ExpBarUI = nil,
    IsLevelUpSelectionActive = false,
    CurrentLevelUpOptions = nil,
    Stats = {}
}

local function captureWorldFromScript(script)
    if script ~= nil and type(script.GetWorld) == "function" then
        local world = script.GetWorld()
        if world ~= nil and world:IsValid() then
            GameManager.World = world
        end
    end
end

function GameManager._ResetState()
    if GameManager.World ~= nil and GameManager.World:IsValid() and GameManager.World.SetGameplayPaused ~= nil then
        GameManager.World:SetGameplayPaused(false)
    end

    GameManager.Stats = {
        MaxHP = 100.0,
        CurrentHP = 100.0,
        MoveSpeedMult = 1.0,
        ExpMult = 1.0,
        PickupRangeMult = 1.0,
        DamageMult = 1.0,
    }

    GameManager.KillCount = 0
    GameManager.IsGameOver = false
    GameManager.IsPaused = false
    GameManager.Initialized = false

    GameManager.SessionPrepared = false
    GameManager.PlayerRegistered = false

    GameManager.PlayerScript = nil
    GameManager.WeaponInventory = nil
    GameManager.LevelSystem = nil
    GameManager.World = nil
    GameManager.LevelUpUI = nil
    GameManager.ExpBarUI = nil
    GameManager.IsLevelUpSelectionActive = false
    GameManager.CurrentLevelUpOptions = nil

    Log("[GameManager] Session State Fully Reset.")
end

function GameManager.PrepareSession(gameTimeLimit)
    GameManager._ResetState()

    GameManager.GameTimeLimit = gameTimeLimit or 600.0
    GameManager.TimeRemaining = GameManager.GameTimeLimit
    GameManager.SessionPrepared = true

    Log("[GameManager] Session Prepared (TimeLimit: " .. tostring(GameManager.GameTimeLimit) .. ")")
    GameManager._CheckAndStart()
end

function GameManager.RegisterPlayer(playerScript)
    GameManager.PlayerScript = playerScript
    captureWorldFromScript(playerScript)

    GameManager.WeaponInventory = WeaponInventory.New(playerScript)
    GameManager.LevelSystem = LevelSystem.New(playerScript, GameManager.WeaponInventory)

    GameManager.PlayerRegistered = true
    Log("[GameManager] Player Registered.")
    GameManager._CheckAndStart()
end

function GameManager._CheckAndStart()
    if GameManager.SessionPrepared and GameManager.PlayerRegistered and not GameManager.Initialized then
        GameManager.Initialized = true
        GameManager.OnGameStart()
    end
end

function GameManager.OnGameStart()
    if GameManager.WeaponInventory then
        GameManager.WeaponInventory:AddWeapon("MainCannon")
    end
    Log("[GameManager] --- GAME START ---")
end

function GameManager.IsGameplayPaused()
    return GameManager.IsPaused == true
end

function GameManager.SetGameplayPaused(paused)
    GameManager.IsPaused = paused == true

    local world = GameManager.World
    if world ~= nil and world:IsValid() and world.SetGameplayPaused ~= nil then
        world:SetGameplayPaused(GameManager.IsPaused)
    end
end

function GameManager.RegisterLevelUpUI(uiScript)
    GameManager.LevelUpUI = uiScript
    captureWorldFromScript(uiScript)

    if GameManager.IsLevelUpSelectionActive and GameManager.CurrentLevelUpOptions ~= nil and
       uiScript ~= nil and type(uiScript.Show) == "function" then
        uiScript:Show(GameManager.CurrentLevelUpOptions)
    end
end

function GameManager.RegisterExpBarUI(expBarScript)
    GameManager.ExpBarUI = expBarScript
    captureWorldFromScript(expBarScript)

    if expBarScript ~= nil and type(expBarScript.SetLevelUpMode) == "function" then
        expBarScript:SetLevelUpMode(GameManager.IsLevelUpSelectionActive)
    end
end

function GameManager.BeginLevelUpSelection(options)
    if options == nil or #options <= 0 then
        return false
    end

    if GameManager.LevelUpUI == nil or type(GameManager.LevelUpUI.Show) ~= "function" then
        Log("[GameManager] LevelUpUI is not registered.")
        return false
    end

    GameManager.CurrentLevelUpOptions = options
    GameManager.IsLevelUpSelectionActive = true
    GameManager.SetGameplayPaused(true)

    if GameManager.ExpBarUI ~= nil and type(GameManager.ExpBarUI.SetLevelUpMode) == "function" then
        GameManager.ExpBarUI:SetLevelUpMode(true)
    end

    GameManager.LevelUpUI:Show(options)
    return true
end

function GameManager.ConfirmLevelUpChoice(index)
    if not GameManager.IsLevelUpSelectionActive or GameManager.LevelSystem == nil then
        return false
    end

    local selected = GameManager.LevelSystem:SelectPendingOption(index)
    if not selected then
        return false
    end

    GameManager.IsLevelUpSelectionActive = false
    GameManager.CurrentLevelUpOptions = nil

    if GameManager.LevelSystem.PendingLevelUps > 0 then
        return GameManager.LevelSystem:RequestLevelUpSelection()
    end

    GameManager.EndLevelUpSelection()
    return true
end

function GameManager.EndLevelUpSelection()
    GameManager.IsLevelUpSelectionActive = false
    GameManager.CurrentLevelUpOptions = nil

    if GameManager.LevelUpUI ~= nil and type(GameManager.LevelUpUI.Hide) == "function" then
        GameManager.LevelUpUI:Hide()
    end

    if GameManager.ExpBarUI ~= nil and type(GameManager.ExpBarUI.SetLevelUpMode) == "function" then
        GameManager.ExpBarUI:SetLevelUpMode(false)
    end

    GameManager.SetGameplayPaused(false)
end

function GameManager.Tick(deltaTime)
    if not GameManager.Initialized or GameManager.IsGameOver or GameManager.IsGameplayPaused() then
        return
    end

    if type(deltaTime) ~= "number" then return end

    GameManager.TimeRemaining = math.max(0, GameManager.TimeRemaining - deltaTime)
    if GameManager.TimeRemaining <= 0 then
        GameManager.OnGameOver("Time Up!")
    end
end

function GameManager.PlayerGetDamage(damage)
    if GameManager.IsGameOver or GameManager.IsGameplayPaused() then return end
    GameManager.Stats.CurrentHP = math.max(0, GameManager.Stats.CurrentHP - damage)
    if GameManager.Stats.CurrentHP <= 0 then GameManager.OnGameOver("Player Died") end
end

function GameManager.OnPickupExp(baseAmount)
    if GameManager.IsGameplayPaused() then return end
    local totalExp = baseAmount * (GameManager.Stats.ExpMult or 1.0)
    if GameManager.LevelSystem then GameManager.LevelSystem:AddExp(totalExp) end
end

function GameManager.OnPickupChest()
    if GameManager.IsGameplayPaused() then return false end
    if GameManager.WeaponInventory then return GameManager.WeaponInventory:OpenChest() end
    return false
end

function GameManager.OnGameOver(reason)
    if GameManager.IsGameOver then return end
    GameManager.IsGameOver = true
    Log("[GameManager] GAME OVER: " .. tostring(reason))
end

return GameManager
