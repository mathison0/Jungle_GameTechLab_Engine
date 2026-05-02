local EnemyAI = require("AI.EnemyAI")

local Script = {}

function Script:BeginPlay()
    self.ai = self:start_coroutine(function()
        EnemyAI.Patrol(self)
    end)
end

function Script:Tick(deltaTime)
end

function Script:EndPlay()
    if self.ai ~= nil then
        self:stop_coroutine(self.ai)
        self.ai = nil
    end
end

return Script