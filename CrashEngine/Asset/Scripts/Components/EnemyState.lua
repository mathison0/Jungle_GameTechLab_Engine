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
    
    -- 데미지 시스템에 등록
    DamageSystem.Register(self.actor, self)
    
    print("EnemyState initialized: " .. self.actor:GetName())
end

function EnemyState:TakeDamage(amount, attacker)
    self.CurrentHP = self.CurrentHP - amount
    print(string.format("[%s] Took %d damage. HP: %d/%d", 
        self.actor:GetName(), amount, self.CurrentHP, self.MaxHP))
    
    if self.CurrentHP <= 0 then
        self:Die()
    end
end

function EnemyState:Die()
    print(self.actor:GetName() .. " died!")
    -- 죽음 처리 (예: 풀에 반환)
    self.actor:SetVisible(false)
    -- self.actor:Destroy() -- 또는 ActorPool 사용 시 Release
end

function EnemyState:EndPlay()
    -- 해제 필수 (메모리 누수 및 잘못된 참조 방지)
    DamageSystem.Unregister(self.actor)
end

return EnemyState
