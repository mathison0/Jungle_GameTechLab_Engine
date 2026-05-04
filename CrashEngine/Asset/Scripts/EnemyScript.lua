local ChaseAI = require("AI.ChaseAI")
local GameManager = require("GameManager")

local Script = {
    properties = ChaseAI.properties
}

function Script:BeginPlay()
    -- GameManager에서 플레이어 액터를 직접 가져옴 (월드 쿼리 비효율 제거)
    if GameManager.PlayerScript then
        self.target = GameManager.PlayerScript:GetActor()
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
