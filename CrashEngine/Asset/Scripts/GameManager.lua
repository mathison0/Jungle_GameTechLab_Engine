local WeaponInventory = require("WeaponInventory")
local LevelSystem = require("LevelSystem")
local Audio = require("Core.Audio")

local BACKGROUND_MUSIC_KEY = "background"
local BACKGROUND_MUSIC_VOLUME = 0.7

local GameManager = {
    GameTimeLimit = 600.0,

    TimeRemaining = 600.0,
    KillCount = 0,
    IsGameOver = false,
    IsPaused = false,

    SessionPrepared = false,
    PlayerRegistered = false,
    GameStartRequested = false,
    Initialized = false,

    PlayerScript = nil,
    WeaponInventory = nil,
    LevelSystem = nil,
    World = nil,
    LevelUpUI = nil,
    ExpBarUI = nil,
    KillCountUI = nil,
    ResultUI = nil,
    MainMenuController = nil,
    BackgroundMusicHandle = nil,
    BackgroundMusicPositionMs = 0.0,
    IsLevelUpSelectionActive = false,
    CurrentLevelUpOptions = nil,
    LastResult = nil,
    Stats = {}
}

local GAMEPLAY_POOL_CLASSES = {
    "AGroundEnemyActor",
    "AFlyingWaveEnemyActor",
    "AEnemyBaseActor",
    "AProjectileActor",
    "AHomingMissileActor",
    "APickupActor",
    "ADeathDecalActor",
}

local function captureWorldFromScript(script)
    if script ~= nil and type(script.GetWorld) == "function" then
        local world = script.GetWorld()
        if world ~= nil and world:IsValid() then
            GameManager.World = world
            if world.SetGameplayPaused ~= nil then
                world:SetGameplayPaused(GameManager.IsPaused == true)
            end
        end
    end
end

local function normalizeSoundPosition(key, positionMs)
    local position = positionMs or 0.0
    local duration = Audio.GetLength(key) or 0.0

    if duration > 0.0 then
        position = position % duration
    end

    return position
end

local function resetStats()
    GameManager.Stats = {
        MaxHP = 100.0,
        CurrentHP = 100.0,
        MoveSpeedMult = 1.0,
        ExpMult = 1.0,
        PickupRangeMult = 1.0,
        DamageMult = 1.0,
    }
end

local function stopAllWeapons()
    if GameManager.WeaponInventory == nil or GameManager.WeaponInventory.Weapons == nil then
        return
    end

    for _, weapon in pairs(GameManager.WeaponInventory.Weapons) do
        if weapon.Stop ~= nil then
            weapon:Stop()
        end
    end
end

local function cleanupGameplayActors()
    if type(GetActorPoolManager) ~= "function" then
        return
    end

    local pool = GetActorPoolManager()
    if pool == nil or not pool:IsValid() or pool.ReleaseActiveByClass == nil then
        return
    end

    for _, className in ipairs(GAMEPLAY_POOL_CLASSES) do
        pool:ReleaseActiveByClass(className)
    end
end

local function updateKillCountUI()
    if GameManager.KillCountUI ~= nil and type(GameManager.KillCountUI.SetKillCount) == "function" then
        GameManager.KillCountUI:SetKillCount(GameManager.KillCount or 0)
    end
end

local function resetRunState()
    stopAllWeapons()
    resetStats()

    GameManager.TimeRemaining = GameManager.GameTimeLimit or 600.0
    GameManager.KillCount = 0
    GameManager.IsGameOver = false
    GameManager.IsLevelUpSelectionActive = false
    GameManager.CurrentLevelUpOptions = nil
    GameManager.LastResult = nil

    if GameManager.LevelUpUI ~= nil and type(GameManager.LevelUpUI.Hide) == "function" then
        GameManager.LevelUpUI:Hide()
    end

    if GameManager.ExpBarUI ~= nil and type(GameManager.ExpBarUI.SetLevelUpMode) == "function" then
        GameManager.ExpBarUI:SetLevelUpMode(false)
    end

    if GameManager.PlayerScript ~= nil then
        GameManager.WeaponInventory = WeaponInventory.New(GameManager.PlayerScript)
        GameManager.LevelSystem = LevelSystem.New(GameManager.PlayerScript, GameManager.WeaponInventory)
    else
        GameManager.WeaponInventory = nil
        GameManager.LevelSystem = nil
    end

    updateKillCountUI()
end

local function buildFallbackResult(resultType, reason)
    return {
        ResultType = resultType,
        Reason = reason,
        RemainingTime = math.max(0.0, tonumber(GameManager.TimeRemaining) or 0.0),
        KillCount = GameManager.KillCount or 0,
        Rank = 0,
        Records = {},
    }
end

function GameManager.PlayBackgroundMusic()
    if GameManager.IsGameOver then
        return
    end

    if GameManager.BackgroundMusicHandle ~= nil and Audio.IsPlaying(GameManager.BackgroundMusicHandle) then
        return
    end

    local handle = Audio.PlayLoop(BACKGROUND_MUSIC_KEY, Audio.Bus.BGM, BACKGROUND_MUSIC_VOLUME)
    GameManager.BackgroundMusicHandle = handle

    if handle ~= nil then
        Audio.SetPosition(handle, normalizeSoundPosition(BACKGROUND_MUSIC_KEY, GameManager.BackgroundMusicPositionMs))
    end
end

function GameManager.PauseBackgroundMusic()
    local handle = GameManager.BackgroundMusicHandle
    if handle ~= nil then
        local position = Audio.GetPosition(handle)
        if position ~= nil then
            GameManager.BackgroundMusicPositionMs = normalizeSoundPosition(BACKGROUND_MUSIC_KEY, position)
        end

        Audio.Stop(handle)
    end

    GameManager.BackgroundMusicHandle = nil
end

function GameManager.StopBackgroundMusic(resetPosition)
    local handle = GameManager.BackgroundMusicHandle
    if handle ~= nil then
        if resetPosition ~= true then
            local position = Audio.GetPosition(handle)
            if position ~= nil then
                GameManager.BackgroundMusicPositionMs = normalizeSoundPosition(BACKGROUND_MUSIC_KEY, position)
            end
        end

        Audio.Stop(handle)
    end

    GameManager.BackgroundMusicHandle = nil

    if resetPosition == true then
        GameManager.BackgroundMusicPositionMs = 0.0
    end
end

function GameManager._ResetState()
    GameManager.StopBackgroundMusic(true)

    if GameManager.World ~= nil and GameManager.World:IsValid() and GameManager.World.SetGameplayPaused ~= nil then
        GameManager.World:SetGameplayPaused(false)
    end

    resetStats()

    GameManager.KillCount = 0
    GameManager.IsGameOver = false
    GameManager.IsPaused = false
    GameManager.Initialized = false

    GameManager.SessionPrepared = false
    GameManager.PlayerRegistered = false
    GameManager.GameStartRequested = false

    GameManager.PlayerScript = nil
    GameManager.WeaponInventory = nil
    GameManager.LevelSystem = nil
    GameManager.World = nil
    GameManager.LevelUpUI = nil
    GameManager.ExpBarUI = nil
    GameManager.KillCountUI = nil
    GameManager.ResultUI = nil
    GameManager.MainMenuController = nil
    GameManager.BackgroundMusicHandle = nil
    GameManager.BackgroundMusicPositionMs = 0.0
    GameManager.IsLevelUpSelectionActive = false
    GameManager.CurrentLevelUpOptions = nil
    GameManager.LastResult = nil

    Log("[GameManager] Session State Fully Reset.")
end

function GameManager.PrepareSession(gameTimeLimit)
    GameManager._ResetState()

    GameManager.GameTimeLimit = gameTimeLimit or 600.0
    GameManager.TimeRemaining = GameManager.GameTimeLimit
    GameManager.SessionPrepared = true
    GameManager.GameStartRequested = true

    Log("[GameManager] Session Prepared (TimeLimit: " .. tostring(GameManager.GameTimeLimit) .. ")")
    GameManager._CheckAndStart()
end

function GameManager.PrepareMainMenu(gameTimeLimit)
    GameManager._ResetState()

    GameManager.GameTimeLimit = gameTimeLimit or 600.0
    GameManager.TimeRemaining = GameManager.GameTimeLimit
    GameManager.SessionPrepared = false
    GameManager.GameStartRequested = false
    GameManager.SetGameplayPaused(true)

    Log("[GameManager] Main Menu Prepared (TimeLimit: " .. tostring(GameManager.GameTimeLimit) .. ")")
end

function GameManager.RequestStartGame()
    if GameManager.Initialized then
        return true
    end

    cleanupGameplayActors()
    resetRunState()

    GameManager.SessionPrepared = true
    GameManager.GameStartRequested = true

    if GameManager.ResultUI ~= nil and type(GameManager.ResultUI.Hide) == "function" then
        GameManager.ResultUI:Hide()
    end

    GameManager._CheckAndStart()
    return GameManager.Initialized == true
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
    if GameManager.SessionPrepared and GameManager.PlayerRegistered and GameManager.GameStartRequested and not GameManager.Initialized then
        GameManager.SetGameplayPaused(false)
        GameManager.Initialized = true
        GameManager.OnGameStart()
    end
end

function GameManager.OnGameStart()
    GameManager.PlayBackgroundMusic()

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

function GameManager.RegisterKillCountUI(uiScript)
    GameManager.KillCountUI = uiScript
    captureWorldFromScript(uiScript)

    if uiScript ~= nil and type(uiScript.SetKillCount) == "function" then
        uiScript:SetKillCount(GameManager.KillCount or 0)
    end
end

function GameManager.RegisterResultUI(uiScript)
    GameManager.ResultUI = uiScript
    captureWorldFromScript(uiScript)

    if GameManager.LastResult ~= nil and uiScript ~= nil and type(uiScript.ShowResult) == "function" then
        uiScript:ShowResult(GameManager.LastResult)
    end
end

function GameManager.UnregisterResultUI(uiScript)
    if GameManager.ResultUI == uiScript then
        GameManager.ResultUI = nil
    end
end

function GameManager.RegisterMainMenuController(controller)
    GameManager.MainMenuController = controller
    captureWorldFromScript(controller)
end

function GameManager.UnregisterKillCountUI(uiScript)
    if GameManager.KillCountUI == uiScript then
        GameManager.KillCountUI = nil
    end
end

function GameManager.OnEnemyKilled(enemyActor)
    if GameManager.IsGameOver then
        return GameManager.KillCount or 0
    end

    GameManager.KillCount = (GameManager.KillCount or 0) + 1

    if GameManager.KillCountUI ~= nil and type(GameManager.KillCountUI.SetKillCount) == "function" then
        GameManager.KillCountUI:SetKillCount(GameManager.KillCount)
    end

    return GameManager.KillCount
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
    GameManager.PauseBackgroundMusic()
    Audio.Play("levelup", Audio.Bus.UI, 1.0)

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
    GameManager.PlayBackgroundMusic()
end

function GameManager.Tick(deltaTime)
    if not GameManager.Initialized or GameManager.IsGameOver or GameManager.IsGameplayPaused() then
        return
    end

    if type(deltaTime) ~= "number" then return end

    GameManager.TimeRemaining = math.max(0, GameManager.TimeRemaining - deltaTime)
    if GameManager.TimeRemaining <= 0 then
        GameManager.OnGameClear()
    end
end

function GameManager.PlayerGetDamage(damage)
    if GameManager.IsGameOver or GameManager.IsGameplayPaused() then return end
    GameManager.Stats.CurrentHP = math.max(0, GameManager.Stats.CurrentHP - damage)

    -- 피격 시 0.1초 동안 빨간색으로 표시
    if GameManager.PlayerScript and GameManager.PlayerScript.StartCoroutine then
        GameManager.PlayerScript:StartCoroutine(function()
            local visual = GameManager.PlayerScript:GetComponentByName("UStaticMeshComponent", "BodyMesh")
            local turret = GameManager.PlayerScript:GetComponentByName("UStaticMeshComponent", "Visual_MainCannon_0")

            local comps = {}
            if visual and visual:IsValid() then table.insert(comps, visual) end
            if turret and turret:IsValid() then table.insert(comps, turret) end

            for _, comp in ipairs(comps) do
                comp:SetMaterialVector4Parameter(0, "SectionColor", {1.0, 0.0, 0.0, 1.0})
            end

            coroutine.yield("wait_time", 0.1)

            for _, comp in ipairs(comps) do
                if comp:IsValid() then
                    comp:SetMaterialVector4Parameter(0, "SectionColor", {1.0, 1.0, 1.0, 1.0})
                end
            end
        end)
    end

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

function GameManager.FinishRun(resultType, reason)
    if GameManager.IsGameOver then return end
    GameManager.IsGameOver = true
    GameManager.Initialized = false
    GameManager.SetGameplayPaused(true)
    GameManager.StopBackgroundMusic(true)
    stopAllWeapons()

    if GameManager.LevelUpUI ~= nil and type(GameManager.LevelUpUI.Hide) == "function" then
        GameManager.LevelUpUI:Hide()
    end

    if GameManager.ExpBarUI ~= nil and type(GameManager.ExpBarUI.SetLevelUpMode) == "function" then
        GameManager.ExpBarUI:SetLevelUpMode(false)
    end

    cleanupGameplayActors()

    local remainingTime = math.max(0.0, tonumber(GameManager.TimeRemaining) or 0.0)
    local killCount = GameManager.KillCount or 0
    local record = nil
    if type(ScoreBoard_AddRecord) == "function" then
        record = ScoreBoard_AddRecord(resultType, remainingTime, killCount)
    end

    if record == nil then
        record = buildFallbackResult(resultType, reason)
    else
        record.Reason = reason
        record.ResultType = record.ResultType or resultType
        record.RemainingTime = record.RemainingTime or remainingTime
        record.KillCount = record.KillCount or killCount
        record.Records = record.Records or {}
    end

    GameManager.LastResult = record

    if GameManager.ResultUI ~= nil and type(GameManager.ResultUI.ShowResult) == "function" then
        GameManager.ResultUI:ShowResult(record)
    end

    Log("[GameManager] " .. tostring(resultType) .. ": " .. tostring(reason))
end

function GameManager.OnGameClear()
    GameManager.TimeRemaining = 0.0
    GameManager.FinishRun("GameClear", "Time Cleared")
end

function GameManager.OnGameOver(reason)
    GameManager.FinishRun("GameOver", reason or "Game Over")
end

function GameManager.ShowScoreBoard()
    if GameManager.ResultUI ~= nil and type(GameManager.ResultUI.ShowScoreBoard) == "function" then
        GameManager.ResultUI:ShowScoreBoard()
        return true
    end

    return false
end

function GameManager.ReturnToMainMenu()
    if GameManager.ResultUI ~= nil and type(GameManager.ResultUI.Hide) == "function" then
        GameManager.ResultUI:Hide()
    end

    cleanupGameplayActors()
    resetRunState()

    GameManager.SessionPrepared = false
    GameManager.GameStartRequested = false
    GameManager.Initialized = false
    GameManager.SetGameplayPaused(true)
    GameManager.StopBackgroundMusic(true)

    if GameManager.MainMenuController ~= nil and type(GameManager.MainMenuController.ShowMenu) == "function" then
        GameManager.MainMenuController:ShowMenu()
    end
end

return GameManager
