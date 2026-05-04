local ChaseAI = require("AI.ChaseAI")
local GameManager = require("GameManager")

local Script = {
    properties = ChaseAI.properties
}

function Script:BeginPlay()
    -- GameManager에서 플레이어 액터를 가져오되, 세션 간 잔류 데이터로 인한 에러 방지
    if GameManager.PlayerScript and type(GameManager.PlayerScript.GetActor) == "function" then
        self.target = GameManager.PlayerScript:GetActor()
    end

    -- 만약 GameManager를 통해 못 찾았다면 직접 태그로 검색 (세션 초기화 지연 대응)
    if not self.target or not self.target:IsValid() then
        self.target = self:QueryActorByTagClosest("Player", {x=0, y=0, z=0}, 1000000.0)
    end

    self.ai = self.StartCoroutine(function()
        ChaseAI.ChaseTargetCoroutine(self, "Player", self.GetRootComponent())
    end)
end

function Script:Tick(deltaTime)
end

function Script:EndPlay()
    self.StopAllCoroutines()
end

return Script
