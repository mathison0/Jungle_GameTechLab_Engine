local DamageSystem = require("Core.DamageSystem")

local EnemyState = {
    properties = {
        MaxHP = { type = "float", default = 100.0 },
        CurrentHP = { type = "float", default = 100.0 }
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
    self.actor:SetVisible(false)
end

function EnemyState:EndPlay()
    if self.actor ~= nil and self.actor:IsValid() then
        DamageSystem.Unregister(self.actor)
    end
end

return EnemyState