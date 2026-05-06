-- PrologueCinematic.lua
-- Attach this to one always-active actor with ULuaScriptComponent.
-- It orchestrates the prologue dialogue and camera sequence without RmlUi or C++ scene logic.

local started = false
local CAMERA_MODIFIER_SCRIPT = "Asset/Scripts/PrologueCinematicCamera.lua"

local function WaitForDialogue()
    while IsDialogueActive() do
        wait(0.1)
    end
end

local function WaitSeconds(seconds)
    local remaining = seconds
    while remaining > 0.0 do
        local step = math.min(remaining, 0.1)
        wait(step)
        remaining = remaining - step
    end
end

local function PlayCameraMove(cameraManager, fromView, toLocation, lookAt, duration)
    local toView = MakeCameraView(toLocation, lookAt, fromView.FOV)
    toView.AspectRatio = fromView.AspectRatio
    toView.NearPlane = fromView.NearPlane
    toView.FarPlane = fromView.FarPlane
    toView.OrthoWidth = fromView.OrthoWidth
    toView.OrthoHeight = fromView.OrthoHeight
    toView.bOrthographic = fromView.bOrthographic

    cameraManager:StartCameraTransition(fromView, toView, duration)
    WaitSeconds(duration)
    return toView
end

local function RunPrologueCinematic()
    SetCinematicInputBlocked(true)

    local cameraManager = GetPlayerCameraManager()
    if cameraManager == nil then
        Log("[PrologueCinematic] No player camera manager.")
        SetCinematicInputBlocked(false)
        return
    end

    SetUIState("Prologue")
    ShowDialogue("Narrator", "Replace this line with the first prologue sentence.")
    QueueDialogue("Narrator", "Replace this line with the second prologue sentence.")
    WaitForDialogue()

    SetUIState("InGame")
    AddLuaCameraModifier(CAMERA_MODIFIER_SCRIPT)

    cameraManager:SetLetterBox(0.12)
    cameraManager:StartCameraFade(FVector(0.0, 0.0, 0.0), 1.0, 0.0, 1.5, false)
    WaitSeconds(1.5)

    local lookAt = FVector(0.0, 0.0, 1.2)
    local view = cameraManager:GetCameraView()

    view = PlayCameraMove(cameraManager, view, FVector(-3.0, -2.0, 1.5), lookAt, 2.5)
    view = PlayCameraMove(cameraManager, view, FVector( 0.0, -3.2, 1.7), lookAt, 2.5)
    view = PlayCameraMove(cameraManager, view, FVector( 3.0, -1.5, 1.5), lookAt, 2.5)
    view = PlayCameraMove(cameraManager, view, FVector( 2.2,  1.8, 1.6), lookAt, 2.5)

    cameraManager:StartCameraFade(FVector(0.0, 0.0, 0.0), 0.0, 1.0, 0.8, true)
    WaitSeconds(0.8)
    cameraManager:ClearLetterBox()
    cameraManager:StartCameraFade(FVector(0.0, 0.0, 0.0), 1.0, 0.0, 0.6, false)

    SetCinematicInputBlocked(false)
end

function BeginPlay(owner)
    if started then
        return
    end

    started = true
    StartCoroutine(RunPrologueCinematic)
end

function EndPlay(owner)
    SetCinematicInputBlocked(false)
end
