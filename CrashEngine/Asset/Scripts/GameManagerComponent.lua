--[[

Property 노출 등을 위한 GameManager의 인터페이스용 인스턴스

]]

---@class GameManagerComponent : ScriptComponent
local Script = {
    properties = {
        GameTimeLimit = { type = "float", default = 600.0, min = 10.0, max = 3600.0, speed = 10.0 },
    }
}

local GameManager = require("GameManager")

function Script:BeginPlay()
    Log("GameManagerComponent: BeginPlay")
    
    -- 플레이어 찾기
    local world = self:GetWorld()
    if not world or not world:IsValid() then
        Log("GameManagerComponent: Invalid World")
        return
    end

    -- 플레이어 태그를 가진 액터 탐색
    local playerActor = self:QueryActorByTagClosest("Player", {x=0, y=0, z=0}, 1000000.0)
    if not playerActor or not playerActor:IsValid() then
        Log("GameManagerComponent: Player not found!")
        -- 플레이어가 아직 생성되지 않았을 수 있으므로, 
        -- 실제 프로젝트에서는 PlayerController 등에서 역으로 등록하거나 
        -- 약간의 지연 후 다시 찾을 필요가 있을 수 있음.
        return
    end

    -- 플레이어의 스크립트 컴포넌트 가져오기 (UScriptComponent)
    local playerScript = playerActor:GetComponent("UScriptComponent")
    if not playerScript then
        Log("GameManagerComponent: Player script (UScriptComponent) not found!")
        return
    end

    -- GameManager 초기화 및 시작
    GameManager.Init(playerScript, self.GameTimeLimit)
    GameManager.OnGameStart()
end

function Script:Tick(deltaTime)
    GameManager.Tick(deltaTime)
end

function Script:EndPlay()
    Log("GameManagerComponent: EndPlay")
end

return Script
