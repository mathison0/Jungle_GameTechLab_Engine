-- LuaScriptComponent contract
-- owner: AActor bound from C++
-- otherActor: AActor or nil
-- hit: FHitResult
-- Log(message): writes to the editor console
-- StartCoroutine(function() ... end), wait(seconds): coroutine helpers

local currentTool = nil
local isCleaning = false

function BeginPlay(owner)
    Log("Player Script Started: " .. owner:GetName())
end

function EndPlay(owner)
end

function OnOverlap(owner, otherActor)
end

function OnEndOverlap(owner, otherActor)
end

function OnHit(owner, hit)
end

function OnInteract(owner, interactor)
end

function Tick(owner, deltaTime)
    local hit = RaycastFromCenter(300.0)

    if hit and hit:IsValid() and hit:IsDecal() then
        local currentTool = GGameContext:GetCurrentToolData()

        if currentTool then
            SetCurrentItem(currentTool.GetName)

            if GetKeyDown(KEY_E) then
                hit:PaintMask(currentTool.Radius, currentTool.Strength)  -- radius, value(지우는 강도)
                local pct = hit:GetCleanPercentage()
                SetProgress(pct)
            end
        end
       
    end
end
