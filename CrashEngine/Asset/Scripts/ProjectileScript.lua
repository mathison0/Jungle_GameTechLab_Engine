---@class ProjectileScript : ScriptComponent
local Script = {
    properties = {
        LifeTimeSeconds = { type = "float", default = 3.0 }
    }
}

function Script:BeginPlay()
    self.ElapsedTime = 0.0
    self.bReturnedToPool = false
    self.PoolManager = GetActorPoolManager()
end

function Script:ResetLifeTime()
    self.ElapsedTime = 0.0
    self.bReturnedToPool = false
end

function Script:Tick(deltaTime)
    if self.bReturnedToPool then
        self:ResetLifeTime()
    end

    self.ElapsedTime = (self.ElapsedTime or 0.0) + deltaTime

    local LifeTimeSeconds = self.LifeTimeSeconds or 3.0
    if self.ElapsedTime < LifeTimeSeconds then
        return
    end

    local PoolManager = self.PoolManager or GetActorPoolManager()
    if PoolManager ~= nil and not PoolManager:IsValid() then
        PoolManager = GetActorPoolManager()
        self.PoolManager = PoolManager
    end

    if PoolManager == nil or not PoolManager:IsValid() then
        return
    end

    local Owner = self:GetActor()
    if Owner == nil or not Owner:IsValid() then
        return
    end

    self.bReturnedToPool = true
    PoolManager:Release(Owner)
end

function Script:EndPlay()
end

return Script
