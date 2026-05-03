local DamageSystem = require("Core.DamageSystem")

local Bullet = {
    properties = {
        Damage = 100
    }
}

function Bullet:BeginPlay()
    self.actor = self.GetActor()
    Log("[Bullet] Spawned: " .. self.actor:GetName())
end

-- 물리 엔진에 의해 충돌이 발생했을 때 호출됨
function Bullet:OnOverlapBegin(otherCollider)
    local otherActor = otherCollider:GetOwner()
    if not otherActor:IsValid() then return end
    
    local tag = otherActor:GetTag()
    Log(string.format("[Bullet] Overlapped with: %s (Tag: %s)", otherActor:GetName(), tag))

    if tag == "Enemy" then
        -- DamageSystem을 통해 상대방 스크립트의 TakeDamage 호출
        local success = DamageSystem.ApplyDamage(otherActor, self.Damage, self.actor)
        
        if success then
            Log("[Bullet] Damage applied successfully to " .. otherActor:GetName())
        else
            Log("[Bullet] Failed to apply damage. Is EnemyState.lua registered?")
        end
        
        -- 충돌 후 총알 제거
        -- self.actor:SetVisible(false)
    end
end

return Bullet
