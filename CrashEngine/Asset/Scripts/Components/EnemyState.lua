local DamageSystem = require("Core.DamageSystem")

local EnemyState = {
    properties = {
        MaxHP = 100,
        CurrentHP = 100,
        GemClassName = {type = "string", default = "APickupActor"},
        DeathDecalClassName = {type = "string", default = "ADeathDecalActor"}
    }
}

function EnemyState:BeginPlay()
    self.actor = self.GetActor()
    self.CurrentHP = self.MaxHP
    self.bIsDead = false
    
    -- 데미지 시스템에 등록 (UUID 기반 룩업)
    DamageSystem.Register(self.actor, self)
end

function EnemyState:TakeDamage(amount, attacker)
    if self.bIsDead then
        return
    end

    self.CurrentHP = self.CurrentHP - amount
    local attackerName = attacker and attacker:GetName() or "Unknown"
    
    Log(string.format("[Enemy:%s] Received %d damage from [%s]. HP: %d/%d", 
        self.actor:GetName(), amount, attackerName, self.CurrentHP, self.MaxHP))
    
    if self.CurrentHP <= 0 then
        self:Die()
    end
end

function EnemyState:Die()
    if self.bIsDead then
        return
    end

    self.bIsDead = true
    Log("[Enemy] Died: " .. self.actor:GetName())
    
    local PoolManager = GetActorPoolManager()
    if PoolManager:IsValid() then
        local deathLocation = self:GetDeathLocation()
        self:SpawnGem(PoolManager, deathLocation)
        self:SpawnDeathDecal(PoolManager, deathLocation)
        PoolManager:Release(self.actor)
    else
        -- Fallback if PoolManager is not available
        self.actor:SetVisible(false)
    end
end

function EnemyState:EndPlay()
    -- 해제 필수
    DamageSystem.Unregister(self.actor)
end

function EnemyState:GetDeathLocation()
    local owner = self:GetActor()
    if owner:IsValid() then
        return owner:GetLocation()
    end

    return {x = 0, y = 0, z = 0}
end

function EnemyState:SpawnGem(PoolManager, SpawnPos)
    local Gem = PoolManager:Acquire(self.GemClassName)
    if Gem:IsValid() then
        Gem:SetLocation(SpawnPos)
    end
end

function EnemyState:SpawnDeathDecal(PoolManager, SpawnPos)
    if self.DeathDecalClassName == nil or self.DeathDecalClassName == "" then
        return
    end

    local Decal = PoolManager:Acquire(self.DeathDecalClassName)
    if Decal:IsValid() then
        Decal:SetLocation(SpawnPos)
    end
end

return EnemyState
