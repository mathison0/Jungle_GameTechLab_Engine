local Targeting = require("AI.TargetingAI")

local Script = {
    properties = Targeting.properties
}

function Script:BeginPlay()
    Log("[TargetAi] BeginPlay")
    self.ai = self.StartCoroutine(function()
        Targeting.TargetCoroutine(self, self.GetComponentByName("UStaticMeshComponent", "RotateTarget"), "TestEnemy", false)
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
