local DamageSystem = {}

-- UUID -> ScriptInstance 매핑 테이블
local registry = {}

--[[
    스크립트 인스턴스를 데미지 시스템에 등록합니다.
    @param actorHandle: 스크립트가 소유한 액터의 핸들
    @param scriptInstance: TakeDamage(amount, attacker) 함수를 가진 Lua 스크립트 인스턴스 (self)
]]
function DamageSystem.Register(actorHandle, scriptInstance)
    if not actorHandle or not actorHandle:IsValid() then return end
    registry[actorHandle:GetUUID()] = scriptInstance
end

--[[
    등록된 스크립트 인스턴스를 해제합니다.
]]
function DamageSystem.Unregister(actorHandle)
    if not actorHandle then return end
    registry[actorHandle:GetUUID()] = nil
end

--[[
    대상 액터에게 데미지를 전달합니다.
    @param targetActor: 데미지를 받을 액터 핸들
    @param amount: 데미지 양
    @param attacker: 공격자 액터 핸들 (선택 사항)
    @return: 데미지 전달 성공 여부
]]
function DamageSystem.ApplyDamage(targetActor, amount, attacker)
    if not targetActor or not targetActor:IsValid() then return false end
    
    local script = registry[targetActor:GetUUID()]
    if script and script.TakeDamage then
        script:TakeDamage(amount, attacker)
        return true
    end
    
    return false
end

--[[
    범위 내의 모든 대상에게 데미지를 입힙니다. (C++ 공간 쿼리 활용)
    @param world: World 핸들
    @param position: 폭발 중심 (FVector)
    @param radius: 폭발 반지름
    @param amount: 데미지 양
    @param tagFilter: 대상 태그 (예: "Enemy")
    @param attacker: 공격자 액터 핸들
]]
function DamageSystem.ApplyRadialDamage(world, position, radius, amount, tagFilter, attacker)
    if not world or not world:IsValid() then return end
    
    -- Physics2D 기반의 OverlapCircle 사용 (정확한 물리 판정)
    local targets = world:OverlapCircle(position, radius, tagFilter or "")
    for _, actor in ipairs(targets) do
        DamageSystem.ApplyDamage(actor, amount, attacker)
    end
end

return DamageSystem
