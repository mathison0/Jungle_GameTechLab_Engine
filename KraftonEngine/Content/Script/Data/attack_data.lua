-- attack_data.lua — 플레이어 공격 데이터 (판정 스펙 / 스텝 / 체인 구성)
--
-- C++ FAttackDataRegistry 가 로드하는 순수 데이터 테이블 — 로직 없음.
-- 저장하면 핫리로드: 다음 콤보 시작 시점부터 반영 (notify 주입분도 자동 재주입).
-- 로드 실패(문법 오류 등) 시 C++ 는 마지막 정상 데이터를 유지하고 로그만 남긴다.
--
-- ── specs: 판정/밸런스 (AttackTypes.h FAttackSpec 대응) ──
--   range    판정 반경 (m)
--   height   수직 허용 거리 (위층/공중 대상 제외)
--   cone_deg 전방 콘 각도 (전체 각). 360 이상 = 전방위
--   dmg      데미지 배율 (공격자 AttackPower ×)
--   kb       넉백 거리 / kb_dur 넉백 시간
--
-- ── steps: 스텝 정의 (에셋 경로는 여기 한 곳에만) ──
--   montage   에디터 저작 몽타주 (없으면 sequence 로 런타임 생성)
--   sequence  fallback 시퀀스. 시퀀스에 저작 notify 가 없으면 아래 값으로 자동 주입:
--   attack_id 히트 notify 의 spec 키 / hit_frac 히트 위치 (길이 비율)
--   window    콤보 윈도우 { 시작비율, 끝비율 } — 없으면 체인 말단/단발
--   play_rate 재생속도. 숫자 = 고정, { min, max } = 매 재생 균등 랜덤 (기본 1.0)
--             notify/RM 은 길이 비율 기준이라 속도를 바꿔도 히트 타이밍은 안 깨진다
--
-- ── chains: 흐름 구성 — 스텝 id 참조라 재배열이 한 줄 ──
--   체인 칸에 id 1개 대신 배열을 쓰면 랜덤 변주 (직전 모션 반복 회피는 C++ 처리):
--     idle = { "combo_v1", "combo_v2", "combo_v3" }            ← 변주 없음
--     idle = { { "combo_v1", "kick" }, "combo_v2", ... }       ← 1단이 둘 중 랜덤

local BARBARIAN_DIR = "Content/Data/GameJam/Barbarian/"
local MONTAGE_DIR   = "Content/Montages/"

local function seq(name)     return BARBARIAN_DIR .. name .. ".uasset" end
local function montage(name) return MONTAGE_DIR .. name .. "_Montage.uasset" end

return {
    specs = {
        -- shake: 히트 시 카메라 셰이크 강도 (0/생략 = 없음). 피니셔일수록 크게.
        --                 range height cone_deg dmg   kb    kb_dur shake
        attack1     = { range = 5.0, height = 2.5, cone_deg = 360, dmg = 2.5, kb = 6.0, kb_dur = 0.30, shake = 0.25 }, -- 360 High — 3단 분기 대피니셔 (시퀀스에 저작된 id)
        attack2     = { range = 3.5, height = 2.5, cone_deg = 140, dmg = 1.5, kb = 4.0, kb_dur = 0.20, shake = 0.10 }, -- Backhand

        -- 좌클릭 콤보 체인 — 단계가 오를수록 강해진다
        combo1      = { range = 3.5, height = 2.5, cone_deg = 140, dmg = 1.0, kb = 1.5, kb_dur = 0.10, shake = 0.05 },
        combo2      = { range = 3.5, height = 2.5, cone_deg = 140, dmg = 1.2, kb = 2.0, kb_dur = 0.12, shake = 0.08 },
        combo3      = { range = 4.5, height = 2.5, cone_deg = 360, dmg = 2.0, kb = 5.0, kb_dur = 0.25, shake = 0.18 }, -- 3단 피니셔

        -- 진입 컨텍스트 변형
        dash_attack = { range = 4.0, height = 2.5, cone_deg = 140, dmg = 1.5, kb = 3.5, kb_dur = 0.20, shake = 0.12 }, -- 이동 중 콤보 진입 — 돌진 베기
        spin_attack = { range = 4.5, height = 2.5, cone_deg = 360, dmg = 1.8, kb = 4.0, kb_dur = 0.20, shake = 0.13 }, -- 이동 중 강공격 — 전진 회전베기
        jump_attack = { range = 4.5, height = 3.5, cone_deg = 360, dmg = 2.0, kb = 4.5, kb_dur = 0.25, shake = 0.15 }, -- 공중 — 도약 내려찍기 (height 여유)

        -- 콤보 분기 피니셔 (□..△) — 깊을수록 강력. 3단 분기는 attack1 재사용
        branch1     = { range = 4.0, height = 2.5, cone_deg = 140, dmg = 1.4, kb = 3.0, kb_dur = 0.18, shake = 0.12 }, -- Horizontal 와이드 횡베기
        -- 2단 분기 = launcher: 로우 스핀으로 주변을 띄운다 (launch = 적, self_launch = 플레이어).
        -- 발동 순간 플레이어도 같이 솟구쳐 점프 없이 공중 체인으로 직행 — 저글 시작.
        -- 넉백은 줄여서 적이 머리 위에 머물게.
        branch2     = { range = 4.5, height = 2.0, cone_deg = 360, dmg = 1.8, kb = 1.5, kb_dur = 0.22, shake = 0.15,
                        launch = 8.0, self_launch = 7.5 }, -- 360 Low 로우 스핀 (launcher)

        -- 공중 체인 — height 여유 (플레이어가 공중이라 위아래로 넓게 판정)
        air1        = { range = 4.0, height = 3.5, cone_deg = 360, dmg = 1.2, kb = 1.0, kb_dur = 0.10, shake = 0.08, launch = 5.0 }, -- 공중 1타 — 재띄움 (저글 유지)
        air2        = { range = 4.0, height = 3.5, cone_deg = 360, dmg = 1.4, kb = 1.5, kb_dur = 0.12, shake = 0.10, launch = 5.0 }, -- 공중 2타

        -- 무쌍기 마무리 강타 — 광범위 전방위 + 강넉백 + 띄움 (난무의 방점)
        musou_slam  = { range = 7.0, height = 3.0, cone_deg = 360, dmg = 3.0, kb = 7.0, kb_dur = 0.30, shake = 0.4, launch = 6.0 },
    },

    steps = {
        -- 기존 3단 콤보 (몽타주/notify 에디터 저작 완료 — 주입 안 함)
        -- 단계가 오를수록 살짝 빨라진다 + 매 재생 ±5% 지터로 기계적인 반복감 제거
        combo_v1      = { montage = montage("Barbarian_Melee Combo Attack Ver. 1"), blend_in = 0.1,
                          play_rate = { 0.95, 1.05 } },
        combo_v2      = { montage = montage("Barbarian_Melee Combo Attack Ver. 2"), blend_in = 0.1,
                          play_rate = { 1.00, 1.10 } },
        combo_v3      = { montage = montage("Barbarian_Melee Combo Attack Ver. 3"), blend_in = 0.1,
                          play_rate = 1.10 },

        -- 콤보 변주 (체인 칸에서 기존 단계와 랜덤 교차) — 시퀀스에 notify 없음 → 주입
        kick          = { montage = montage("Barbarian_Melee Attack Kick Ver. 2"),       -- 1.40s, 제자리 킥
                          sequence = seq("Barbarian_Melee Attack Kick Ver. 2"),
                          blend_in = 0.1, attack_id = "combo1", hit_frac = 0.40, window = { 0.55, 0.85 },
                          play_rate = { 0.95, 1.05 } },
        gs_slash4     = { montage = montage("great sword slash (4)_mixamo_com"),         -- 1.80s, 전진 +1.1m 베기
                          sequence = seq("great sword slash (4)_mixamo_com"),
                          blend_in = 0.1, attack_id = "combo2", hit_frac = 0.40, window = { 0.55, 0.85 },
                          play_rate = { 1.00, 1.10 } },

        -- 이동 진입 — 돌진 베기 (RM +3.56m)
        slide         = { montage = montage("great sword slide attack_mixamo_com"),
                          sequence = seq("great sword slide attack_mixamo_com"),
                          blend_in = 0.1, attack_id = "dash_attack", hit_frac = 0.35, window = { 0.55, 0.85 } },

        -- 이동 진입 변주 — 한손 돌진 베기 (2.33s, RM +3.69m, slide 와 프로필 유사)
        ss_lunge      = { montage = montage("sword and shield attack_mixamo_com"),
                          sequence = seq("sword and shield attack_mixamo_com"),
                          blend_in = 0.1, attack_id = "dash_attack", hit_frac = 0.45, window = { 0.60, 0.85 },
                          play_rate = { 1.00, 1.10 } },

        -- 공중 — 도약 내려찍기 (RM 전진 +3.25m, 착지 후에도 끝까지 재생). 공중 체인 피니셔
        jump          = { montage = montage("great sword jump attack_mixamo_com"),
                          sequence = seq("great sword jump attack_mixamo_com"),
                          blend_in = 0.15, attack_id = "jump_attack", hit_frac = 0.45 },

        -- 공중 체인 1/2타 — 제자리 휘두르기 (RM 없음, 행 타임 동안 천천히 낙하하며 연계)
        air_slash1    = { montage = montage("great sword slash (5)_mixamo_com"),
                          sequence = seq("great sword slash (5)_mixamo_com"),
                          blend_in = 0.1, attack_id = "air1", hit_frac = 0.40, window = { 0.50, 0.85 },
                          play_rate = { 1.05, 1.15 } },
        air_slash2    = { montage = montage("great sword attack_mixamo_com"),
                          sequence = seq("great sword attack_mixamo_com"),
                          blend_in = 0.1, attack_id = "air2", hit_frac = 0.40, window = { 0.50, 0.85 },
                          play_rate = { 1.05, 1.15 } },

        -- 강공격
        backhand      = { montage = montage("Barbarian_Melee Attack Backhand"), blend_in = 0.2 },
        spin_high_fwd = { montage = montage("great sword high spin attack_mixamo_com"),
                          sequence = seq("great sword high spin attack_mixamo_com"),
                          blend_in = 0.15, attack_id = "spin_attack", hit_frac = 0.40 },

        -- 분기 피니셔 (Horizontal/360 Low 는 시퀀스에 notify 없음 → 주입, 360 High 는 attack1 저작본 존중)
        horizontal    = { montage = montage("Barbarian_Melee Attack Horizontal"),
                          sequence = seq("Barbarian_Melee Attack Horizontal"),
                          blend_in = 0.1, attack_id = "branch1", hit_frac = 0.40 },
        spin_low      = { montage = montage("Barbarian_Melee Attack 360 Low"),
                          sequence = seq("Barbarian_Melee Attack 360 Low"),
                          blend_in = 0.1, attack_id = "branch2", hit_frac = 0.45 },
        spin_high     = { montage = montage("Barbarian_Melee Attack 360 High"),
                          sequence = seq("Barbarian_Melee Attack 360 High"),
                          blend_in = 0.1, attack_id = "attack1", hit_frac = 0.45 },

        -- 무쌍기 마무리 — Downward 내려찍기 (2.27s, 제자리 강타). 난무 전용
        u_slam        = { montage = montage("Barbarian_Melee Attack Downward"),
                          sequence = seq("Barbarian_Melee Attack Downward"),
                          blend_in = 0.1, attack_id = "musou_slam", hit_frac = 0.50,
                          play_rate = 1.05 },
    },

    chains = {
        -- 좌클릭 콤보 — 진입 컨텍스트별. 이동 중엔 1단이 돌진 베기로 바뀌고 2단부터 합류.
        -- 1/2단은 변주 배열 — 매 콤보 랜덤 (직전 모션 반복 회피). 3단 피니셔는 고정이 연출상 안정적.
        light = {
            idle   = { { "combo_v1", "kick" }, { "combo_v2", "gs_slash4" }, "combo_v3" },
            moving = { { "slide", "ss_lunge" }, { "combo_v2", "gs_slash4" }, "combo_v3" },

            -- 일반 점프 공격 — 단발 내려찍기 (기존 동작). 행 타임 없음.
            air        = { "jump" },

            -- launcher(branch2 self_launch) 로 떠올랐을 때만 — 공중 저글 3단.
            -- 슬래시 2연타(재띄움으로 저글 유지, 정점 이후 행 타임) → 내려찍기 피니셔로 착지.
            air_juggle = { "air_slash1", "air_slash2", "jump" },
        },

        -- 우클릭 강공격 — 컨텍스트별 단발
        heavy = {
            idle   = "backhand",
            moving = "spin_high_fwd",
            air    = "jump",   -- 전용 모션 확보 시 교체
        },

        -- 콤보 분기 피니셔 (□..△) — 인덱스 = 분기 시점 단수. 단수 초과 시 마지막으로 clamp
        branch = { "horizontal", "spin_low", "spin_high" },

        -- 무쌍기 (R, 게이지 가득) — 난무: 슬롯 순차 자동 재생, 전 구간 무적.
        -- ※ spin_low(self_launch) 처럼 자기 띄움 있는 스텝은 넣지 말 것 — 난무가 공중으로 끊긴다.
        ultimate = { "horizontal", "spin_high", "u_slam" },
    },

    -- ── 전투 피드백/연출 (AMusouGameMode / AMusouCharacter 소비) ──
    feedback = {
        -- 킬 버스트 — 스윙 1회 판정으로 min_kills 이상 처치 시 글로벌 슬로모 + 강셰이크
        kill_burst = {
            min_kills  = 5,
            slomo_dur  = 0.25,  -- 슬로모 지속 (실시간 초)
            slomo_rate = 0.25,  -- 타임스케일 (0..1, 낮을수록 느려짐)
            shake      = 0.4,   -- 버스트 카메라 셰이크 강도
        },

        -- 공중 콤보 행 타임 — 공중 체인 진행 중 플레이어 중력 배율 (1 = 그대로)
        air_combo = {
            gravity_scale = 0.25,
        },

        -- 무쌍 게이지 — 이 킬 수를 채우면 무쌍기(R) 발동 가능
        ultimate = {
            kills_to_fill = 30,
        },
    },
}
