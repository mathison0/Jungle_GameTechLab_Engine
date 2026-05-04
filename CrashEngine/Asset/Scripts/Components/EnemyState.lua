local DamageSystem = require("Core.DamageSystem")

local EnemyState = {
    properties = {
        MaxHP = 100,
        CurrentHP = 100,
        GemClassName = {type = "string", default = "APickupActor"}
    }
}

function EnemyState:BeginPlay()
    self.actor = self:GetActor()
    if self.actor == nil or not self.actor:IsValid() then
        Log("[EnemyState] Invalid owner actor")
        return
    end

    self.CurrentHP = self.MaxHP or 100.0
    DamageSystem.Register(self.actor, self)
end

function EnemyState:TakeDamage(amount, attacker)
    amount = amount or 0.0

    self.CurrentHP = (self.CurrentHP or self.MaxHP or 100.0) - amount

    local attackerName = "Unknown"
    if attacker ~= nil and attacker:IsValid() then
        attackerName = attacker:GetName()
    end

    Log(string.format("[Enemy:%s] Received %.1f damage from [%s]. HP: %.1f/%.1f",
        self.actor:GetName(), amount, attackerName, self.CurrentHP, self.MaxHP or 100.0))

    if self.CurrentHP <= 0 then
        self:Die()
    end
end

function EnemyState:Die()
    Log("[Enemy] Died: " .. self.actor:GetName())
    
    local PoolManager = GetActorPoolManager()
    if PoolManager:IsValid() then
        self:SpawnGem(PoolManager)
        PoolManager:Release(self.actor)
    else
        -- Fallback if PoolManager is not available
        self.actor:SetVisible(false)
    end
end

function EnemyState:EndPlay()
    if self.actor ~= nil and self.actor:IsValid() then
        DamageSystem.Unregister(self.actor)
    end
end

function EnemyState:SpawnGem(PoolManager)
    local Gem = PoolManager:Acquire(self.GemClassName)
    if Gem:IsValid() then
        local owner = self:GetActor()
        local SpawnPos = {x =0, y=0, z=0}
        if owner:IsValid() then
            SpawnPos = owner:GetLocation()
        end
        Gem:SetLocation(SpawnPos)
    end
end

return EnemyState
