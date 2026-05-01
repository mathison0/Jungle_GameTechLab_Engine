local Script = {}

function Script:BeginPlay()
    Log("Template.lua BeginPlay")
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
    Log("Template.lua EndPlay")
end

return Script
