local ChaseAI = require("AI.ChaseAI")
local GameManager = require("GameManager")

local Script = {
    properties = ChaseAI.properties
}

function Script:BeginPlay()
    -- GameManager가 이번 세션에서 완전히 초기화되었는지 확인 후 플레이어 참조 가져오기
    if GameManager.Initialized and GameManager.PlayerScript and type(GameManager.PlayerScript.GetActor) == "function" then
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
