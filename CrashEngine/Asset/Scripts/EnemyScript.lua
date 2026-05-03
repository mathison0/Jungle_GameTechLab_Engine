local ChaseAI = require("AI.ChaseAI")
local Query = require("Query")


local Script = {
    properties = ChaseAI.properties
}

function Script:BeginPlay()
    Query.SearchActorClosestByTag(self, self.GetRootComponent(), "Tank")
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
