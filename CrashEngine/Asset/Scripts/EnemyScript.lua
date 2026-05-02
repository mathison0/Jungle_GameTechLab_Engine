local EnemyAI = require("AI.EnemyAI")

local Script = {}

function Script:BeginPlay()
    self.ai = self.StartCoroutine(function()
        EnemyAI.ChaseTarget(self, "Player")
    end)
end

function Script:Tick(deltaTime)
end

function Script:EndPlay()
    if self.ai ~= nil then
        self.StopCoroutine(self.ai)
        self.ai = nil
    end
end

return Script
