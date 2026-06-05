-- barbarian_character.lua — Barbarian 플레이어 Pawn 게임 로직
--
-- ULuaScriptComponent 용 액터 레벨 스크립트.
--
-- ※ 히트 판정/데미지는 C++로 이관됨 (군체 Manager 구조 대응):
--   몽타주의 AnimNotify_MusouAttack(AttackId) 발동
--     → AMusouGameMode::BroadcastAttack(FMusouAttackEvent)
--     → 수신자(군체 Manager / 보스 BattleComponent)가 각자 판정·적용
--   공격 스펙(범위/데미지/넉백)은 Source/Game/Musou/Combat/AttackTypes.h 테이블.
--
-- 이 스크립트에 남는 역할:
--   - 액터 태그 등 초기화
--   - (추후) 회피/대시, 스킬 게이지 등 Pawn 레벨 보조 로직

function BeginPlay()
    obj:AddTag("player")
    print("[Barbarian] BeginPlay: " .. obj.Name)
end

function EndPlay()
end

function OnOverlap(OtherActor)
end

function Tick(dt)
end
