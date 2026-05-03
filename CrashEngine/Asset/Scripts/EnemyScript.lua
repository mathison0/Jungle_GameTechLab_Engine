local ChaseAI = require("AI.ChaseAI")

local Script = {
    properties = ChaseAI.properties
}

function Script:BeginPlay()
    self.ai = self.StartCoroutine(function()
        ChaseAI.ChaseTargetCoroutine(self, "Tank", self.GetRootComponent())
    end)
end

function Script:Tick(deltaTime)
end

function Script:EndPlay()
    self.StopAllCoroutines()
end

return Script
