local DamageSystem = require("Core.DamageSystem")

local Bullet = {
    properties = {
        Damage = { type = "float", default = 100.0 }
    }
}

function Bullet:BeginPlay()
    Log("Bullet:BeginPlay")

    self.actor = self:GetActor()
    if self.actor == nil or not self.actor:IsValid() then
        Log("[Bullet] Invalid owner actor")
        return
    end

    Log("[Bullet] Spawned: " .. self.actor:GetName())
end

function Bullet:OnOverlapBegin(otherCollider)
    Log("Bullet:OnOverlapBegin")

    if otherCollider == nil or not otherCollider:IsValid() then
        Log("[Bullet] Invalid other collider")
        return
    end

    if otherCollider.GetOwner == nil then
        Log("[Bullet] otherCollider has no GetOwner()")
        return
    end

    local otherActor = otherCollider:GetOwner()
    if otherActor == nil or not otherActor:IsValid() then
        return
    end

    local tag = otherActor:GetTag()
    Log(string.format("[Bullet] Overlapped with: %s (Tag: %s)", otherActor:GetName(), tag))

    if tag == "Enemy" then
        local damage = self.Damage or 100.0
        local success = DamageSystem.ApplyDamage(otherActor, damage, self.actor)

        if success then
            Log("[Bullet] Damage applied successfully to " .. otherActor:GetName())
        else
            Log("[Bullet] Failed to apply damage. EnemyState may not be registered.")
        end

        if self.actor ~= nil and self.actor:IsValid() then
            self.actor:SetVisible(false)
        end
    end
end

return Bullet
