-- barbarian_character.lua — Barbarian 플레이어 Pawn 게임 로직 (프로토타입)
--
-- ULuaScriptComponent 용 액터 레벨 스크립트. 애님(Anim/player_anim.lua)과 책임 분리:
--   - 이 파일      : 공격 히트 판정, 히트 피드백(히트스탑/넉백), 게임플레이 이벤트
--   - player_anim  : pose/FSM/montage 재생
--
-- 히트 판정 흐름 (프로토타입):
--   좌클릭 = attack1 (Melee Attack 360 High) → 일정 딜레이 후 360도 범위 스윕
--   우클릭 = attack2 (Melee Attack Backhand) → 일정 딜레이 후 전방 콘 스윕
--   대상   = "enemy" 태그가 붙은 액터
--
-- TODO(확장):
--   - 데미지/체력은 C++ UBattleComponent 담당 — lua 바인딩 추가 후 ApplyDamage 연동
--   - 콤보 단계별 히트 타이밍은 montage notify(on_notify) 기반으로 교체
--   - 히트 이펙트/사운드 (AudioManager.Play)

local VK_LBUTTON = 1
local VK_RBUTTON = 2

local ENEMY_TAG = "enemy"

-- ── 공격 정의 — 몽타주 스윙 타이밍에 맞춰 조절 ──
local ATTACKS = {
    attack1 = {
        hit_delay = 0.5,     -- 입력 후 히트 판정까지 (360 High 스윙 타이밍)
        range = 4.0,
        cone_cos = -1.0,     -- 360도 (각도 제한 없음)
        cooldown = 1.2,
        knockback_dist = 2.5,
        knockback_dur = 0.15,
    },
    attack2 = {
        hit_delay = 0.4,     -- Backhand 스윙 타이밍
        range = 3.5,
        cone_cos = 0.34,     -- 전방 약 140도 콘 (cos 70도)
        cooldown = 1.0,
        knockback_dist = 4.0,
        knockback_dur = 0.2,
    },
}

local HIT_STOP_DURATION = 0.08   -- 히트 시 타격감용 히트스탑

local selfAction = nil           -- 내 ActionComponent (히트스탑용)
local pendingHits = {}           -- 예약된 히트 판정 { remaining, attack }
local cooldowns = { attack1 = 0.0, attack2 = 0.0 }

-- ── 히트 스윕 — range 내 + 콘 각도 내의 enemy 태그 액터 타격 ──
local function do_hit_sweep(attack)
    local myPos = obj:GetActorLocation()
    local forward = obj:GetActorForward()

    local enemies = World.FindActorsByTag(ENEMY_TAG)
    local hitCount = 0

    for _, enemy in ipairs(enemies) do
        if enemy ~= nil and enemy:IsValid() then
            local toEnemy = enemy:GetActorLocation() - myPos
            local dist = toEnemy:Length()

            if dist <= attack.range and dist > 0.001 then
                local dir = toEnemy:Normalized()
                if forward:Dot(dir) >= attack.cone_cos then
                    hitCount = hitCount + 1

                    -- 넉백 — 적의 ActionComponent가 있으면 밀어낸다
                    local enemyAction = enemy:GetActionComponent()
                    if enemyAction ~= nil then
                        enemyAction:Knockback(dir, attack.knockback_dist, attack.knockback_dur)
                    end

                    -- TODO: UBattleComponent lua 바인딩 후
                    --       enemy BattleComponent:ApplyDamage(attackPower, obj) 호출
                    print("[Barbarian] hit: " .. enemy.Name)
                end
            end
        end
    end

    -- 하나라도 맞으면 히트스탑으로 타격감
    if hitCount > 0 and selfAction ~= nil then
        selfAction:HitStop(HIT_STOP_DURATION)
    end
end

local function try_attack(name)
    local attack = ATTACKS[name]
    if attack == nil or cooldowns[name] > 0.0 then
        return
    end

    cooldowns[name] = attack.cooldown
    table.insert(pendingHits, { remaining = attack.hit_delay, attack = attack })
end

function BeginPlay()
    obj:AddTag("player")
    selfAction = obj:GetActionComponent()
    print("[Barbarian] BeginPlay: " .. obj.Name)
end

function EndPlay()
end

function OnOverlap(OtherActor)
end

function Tick(dt)
    -- 쿨다운 갱신
    for name, remain in pairs(cooldowns) do
        if remain > 0.0 then
            cooldowns[name] = math.max(0.0, remain - dt)
        end
    end

    -- 공격 입력 — 애님(player_anim.lua)이 같은 입력으로 몽타주를 재생하고,
    -- 여기서는 스윙 타이밍에 맞춰 히트 판정을 예약한다.
    if Input.GetKeyDown(VK_LBUTTON) then
        try_attack("attack1")
    end
    if Input.GetKeyDown(VK_RBUTTON) then
        try_attack("attack2")
    end

    -- 예약된 히트 판정 처리
    for i = #pendingHits, 1, -1 do
        local pending = pendingHits[i]
        pending.remaining = pending.remaining - dt
        if pending.remaining <= 0.0 then
            do_hit_sweep(pending.attack)
            table.remove(pendingHits, i)
        end
    end
end
