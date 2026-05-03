local DamageSystem = require("Core.DamageSystem")

local EnemyState = {
    properties = {
        MaxHP = 100,
        CurrentHP = 100
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
    self.actor:SetVisible(false)
end

function EnemyState:EndPlay()
    -- 해제 필수
    DamageSystem.Unregister(self.actor)
end

return EnemyState
