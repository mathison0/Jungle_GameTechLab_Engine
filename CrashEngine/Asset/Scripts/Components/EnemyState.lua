local DamageSystem = require("Core.DamageSystem")

local EnemyState = {
    properties = {
        MaxHP = 100,
        CurrentHP = 100,
        GemClassName = {type = "string", default = "APickupActor"}
    }
}

function EnemyState:BeginPlay()
    self.actor = self.GetActor()
    self.CurrentHP = self.MaxHP
    
    -- 데미지 시스템에 등록 (UUID 기반 룩업)
    DamageSystem.Register(self.actor, self)
end

function EnemyState:TakeDamage(amount, attacker)
    self.CurrentHP = self.CurrentHP - amount
    local attackerName = attacker and attacker:GetName() or "Unknown"
    
    Log(string.format("[Enemy:%s] Received %d damage from [%s]. HP: %d/%d", 
        self.actor:GetName(), amount, attackerName, self.CurrentHP, self.MaxHP))
    
    if self.CurrentHP <= 0 then
        self:Die()
    end
end

function EnemyState:Die()
    -- 임시로 Visible만 조절
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
    -- 해제 필수
    DamageSystem.Unregister(self.actor)
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
