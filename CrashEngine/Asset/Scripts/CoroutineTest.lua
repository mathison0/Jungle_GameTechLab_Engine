local Script = {}

function Wait(Time)
    coroutine.yield("wait_time", Time)
end

EnemyAI = {}

function EnemyAI.start()
    for i = 1, 3 do
        Wait(2.0)
        Log("[AI] CoroutineCalled")
    end
    Log("[AI] End")

end

function Script:BeginPlay()
    Log("Template.lua BeginPlay")
    self.ai = self:start_coroutine(EnemyAI.start)
end

function Script:Tick(deltaTime)
    if self.elapsed == nil then
        self.elapsed = 0
    end

    self.elapsed = self.elapsed + deltaTime

    if self.elapsed >= 1.0 then
        Log("Template.lua Tick")
        self.elapsed = 0
    end
end

function Script:EndPlay()
    self:stop_coroutine(self.ai)
    Log("Template.lua EndPlay")
end

return Script
