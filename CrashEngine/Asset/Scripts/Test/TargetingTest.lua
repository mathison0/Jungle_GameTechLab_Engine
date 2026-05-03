local Targeting = require("AI.TargetingAI")

local Script = {
    properties = Targeting.properties
}

function Script:BeginPlay()
    self.ai = self.StartCoroutine(function()
        TargetingAI:TargetCoroutine(self.GetomponentByName("RotateTarget"), "Tank", false)
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
