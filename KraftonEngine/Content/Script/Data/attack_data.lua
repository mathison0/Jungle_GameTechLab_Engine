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
--   camera    몽타주 카메라 연출 샷 배열 (선택) — 구간 동안 메인(SpringArm) 카메라
--             대신 캐릭터에 붙은 연출 카메라로 블렌드했다가 복귀:
--               camera = { { begin_frac = 0.1, end_frac = 0.7, ... }, ... }
--             begin_frac/end_frac  샷 구간 (재생 길이 비율, 필수)
--             blend_in/blend_out   진입 / 메인 복귀 블렌드 초 (기본 0.15 / 0.25)
--             offset = {x,y,z}     캡슐 로컬 m — x=전방, y=우측, z=상단 (기본 {2.5,2,1})
--             fov                  도 단위 (0/생략 = 메인 카메라 FOV 유지)
--             look_at              매 프레임 캐릭터 응시 (기본 true)
--             look_height          시선 높이 — 캡슐 중심 +Z m (기본 0.5)
--             rotation = {p,y,r}   look_at = false 일 때의 로컬 회전 (도)
--             follow               false = 샷 시작 위치 월드 고정 (기본 true = 추적)
--             letterbox            시네마틱 바 두께 비율 (0 = 없음)
--             anchor               "camera"(기본) = offset 을 화면 시점 기준 배치 (캐릭터가
--                                  옆을 봐도 일관 + 블렌드 튐 적음) / "character" = 캐릭터 facing
--                                  기준 (도약·돌진 방향을 따라가는 추적샷)
--             연속 컷은 항목 2개 — 연출 카메라 2대가 핑퐁하며 컷 사이도 블렌드.
--             셰이크/슬로모는 연출 중에도 적용. 고빈도 일반 콤보엔 금지 (멀미 주의)
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

        -- 좌클릭 콤보 체인 — 단계가 오를수록 강해진다 (주입형 스텝용)
        light1      = { range = 3.5, height = 2.5, cone_deg = 140, dmg = 1.0, kb = 1.5, kb_dur = 0.10, shake = 0.05 },
        light2      = { range = 3.5, height = 2.5, cone_deg = 140, dmg = 1.2, kb = 2.0, kb_dur = 0.12, shake = 0.08 },
        light3      = { range = 4.5, height = 2.5, cone_deg = 360, dmg = 2.0, kb = 5.0, kb_dur = 0.25, shake = 0.18 }, -- 3단 피니셔

        -- ※ 레거시 — Combo Ver.1/2/3 몽타주의 "저작" notify 가 이 id 들을 참조 (시퀀스에
        --   박혀 있어 lua 에서 못 바꿈). 세 몽타주 모두 현재 3단 피니셔 풀 전용이라
        --   값은 light3(3단급) 와 동일하게 맞춰둠 — 다른 단수로 옮기면 여기도 조정할 것.
        combo1      = { range = 4.5, height = 2.5, cone_deg = 360, dmg = 2.0, kb = 5.0, kb_dur = 0.25, shake = 0.18 },
        combo2      = { range = 4.5, height = 2.5, cone_deg = 360, dmg = 2.0, kb = 5.0, kb_dur = 0.25, shake = 0.18 },
        combo3      = { range = 4.5, height = 2.5, cone_deg = 360, dmg = 2.0, kb = 5.0, kb_dur = 0.25, shake = 0.18 },

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

        -- 공중 강공격 — 도약 내려찍기 강화판 (광역 + 강넉백)
        jump_heavy  = { range = 5.0, height = 3.5, cone_deg = 360, dmg = 2.3, kb = 5.0, kb_dur = 0.25, shake = 0.2 },

        -- 무쌍기 마무리 강타 — 광범위 전방위 + 강넉백 + 띄움 (난무의 방점)
        musou_slam  = { range = 7.0, height = 3.0, cone_deg = 360, dmg = 3.0, kb = 7.0, kb_dur = 0.30, shake = 0.4, launch = 6.0 },

        -- 무쌍기 전방 진행 충격파 펄스 — 좁은 반경으로 "지나가며 한 번" 행진감. 전방 콘으로
        -- 진행 방향만 때린다. shake 는 펄스마다 누적되므로 슬램의 1/5(0.08)로 낮춤.
        musou_wave  = { range = 2.5, height = 3.0, cone_deg = 200, dmg = 1.5, kb = 4.5, kb_dur = 0.20, shake = 0.08, launch = 2.5 },

        -- Boss patterns - boss_data.lua references these by attack_id.
        boss_slash  = { range = 4.8, height = 3.0, cone_deg = 150, dmg = 1.0, kb = 3.5, kb_dur = 0.22, shake = 0.12 },
        boss_spin   = { range = 6.0, height = 3.2, cone_deg = 360, dmg = 1.4, kb = 5.0, kb_dur = 0.30, shake = 0.18 },
        boss_bolt   = { range = 11.0, height = 3.0, cone_deg = 35, dmg = 1.1, kb = 2.0, kb_dur = 0.15, shake = 0.08 },
        boss_nova   = { range = 5.8, height = 3.2, cone_deg = 360, dmg = 1.3, kb = 4.5, kb_dur = 0.25, shake = 0.15 },
        boss_dash_slash = { range = 5.5, height = 3.0, cone_deg = 120, dmg = 1.2, kb = 4.0, kb_dur = 0.25 },
    },

    steps = {
        -- 기존 3단 콤보 (몽타주/notify 에디터 저작 완료 — 주입 안 함)
        -- 단계가 오를수록 살짝 빨라진다 + 매 재생 ±5% 지터로 기계적인 반복감 제거
        combo_v1      = { montage = montage("Barbarian_Melee Combo Attack Ver. 1"), blend_in = 0.1,
                          play_rate = { 1.4, 1.6 } },
        combo_v2      = { montage = montage("Barbarian_Melee Combo Attack Ver. 2"), blend_in = 0.1,
                          play_rate = { 1.4, 1.6 } },
        combo_v3      = { montage = montage("Barbarian_Melee Combo Attack Ver. 3"), blend_in = 0.1,
                          play_rate = { 1.4, 1.6 } },

        -- 콤보 변주 (체인 칸에서 기존 단계와 랜덤 교차) — 시퀀스에 notify 없음 → 주입
        kick          = { montage = montage("Barbarian_Melee Attack Kick Ver. 2"),       -- 1.40s, 제자리 킥
                          sequence = seq("Barbarian_Melee Attack Kick Ver. 2"),
                          blend_in = 0.1, attack_id = "light1", hit_frac = 0.40, window = { 0.55, 0.85 },
                          play_rate = { 0.95, 1.05 } },
        gs_slash4     = { montage = montage("great sword slash (4)_mixamo_com"),         -- 1.80s, 전진 +1.1m 베기
                          sequence = seq("great sword slash (4)_mixamo_com"),
                          blend_in = 0.1, attack_id = "light2", hit_frac = 0.40, window = { 0.55, 0.85 },
                          play_rate = { 1.00, 1.10 } },

        -- idle 1단 변주 후보군 — 전부 제자리 베기 (RM 없음, 1.3~1.5s)
        -- ※ great sword slash (5) 는 air_slash1 이 같은 시퀀스 사용 중 (시퀀스당 주입
        --   attack_id 1개 제한) — idle 후보로 못 씀. 대체 필요 시 slash (3) 검토.
        ss_slash      = { montage = montage("sword and shield slash_mixamo_com"),        -- 1.50s
                          sequence = seq("sword and shield slash_mixamo_com"),
                          blend_in = 0.1, attack_id = "light1", hit_frac = 0.40, window = { 0.55, 0.85 },
                          play_rate = { 0.95, 1.05 } },
        ss_slash5     = { montage = montage("sword and shield slash (5)_mixamo_com"),    -- 1.37s
                          sequence = seq("sword and shield slash (5)_mixamo_com"),
                          blend_in = 0.1, attack_id = "light1", hit_frac = 0.40, window = { 0.55, 0.85 },
                          play_rate = { 0.95, 1.05 } },
        gs_slash      = { montage = montage("great sword slash_mixamo_com"),             -- 1.27s
                          sequence = seq("great sword slash_mixamo_com"),
                          blend_in = 0.1, attack_id = "light1", hit_frac = 0.40, window = { 0.55, 0.85 },
                          play_rate = { 0.95, 1.05 } },

        -- 2단 변주 후보군 (gs_slash4 포함 — idle/moving 공용). 전진 RM 있는 것들은 추격감 부여
        ss_slash3     = { montage = montage("sword and shield slash (3)_mixamo_com"),    -- 1.67s, 제자리
                          sequence = seq("sword and shield slash (3)_mixamo_com"),
                          blend_in = 0.1, attack_id = "light2", hit_frac = 0.40, window = { 0.55, 0.85 },
                          play_rate = { 1.00, 1.10 } },
        ss_atk2       = { montage = montage("sword and shield attack (2)_mixamo_com"),   -- 1.30s, 전진 +3.0m
                          sequence = seq("sword and shield attack (2)_mixamo_com"),
                          blend_in = 0.1, attack_id = "light2", hit_frac = 0.45, window = { 0.55, 0.85 },
                          play_rate = { 1.00, 1.10 } },
        ss_atk3       = { montage = montage("sword and shield attack (3)_mixamo_com"),   -- 1.73s, 전진 +2.5m
                          sequence = seq("sword and shield attack (3)_mixamo_com"),
                          blend_in = 0.1, attack_id = "light2", hit_frac = 0.45, window = { 0.55, 0.85 },
                          play_rate = { 1.00, 1.10 } },
        gs_slash3     = { montage = montage("great sword slash (3)_mixamo_com"),         -- 1.83s, 제자리
                          sequence = seq("great sword slash (3)_mixamo_com"),
                          blend_in = 0.1, attack_id = "light2", hit_frac = 0.40, window = { 0.55, 0.85 },
                          play_rate = { 1.00, 1.10 } },
        inward_slash  = { montage = montage("Barbarian Sword Inward Slash_mixamo_com"),  -- 2.20s, 제자리 (느려서 속도 보정)
                          sequence = seq("Barbarian Sword Inward Slash_mixamo_com"),
                          blend_in = 0.1, attack_id = "light2", hit_frac = 0.40, window = { 0.55, 0.85 },
                          play_rate = { 1.15, 1.25 } },

        -- 3단 변주 후보군 (피니셔) — 윈도우는 3단 분기(□□□△) 입력용. 긴 클립은 속도 보정
        ss_slash4     = { montage = montage("sword and shield slash (4)_mixamo_com"),    -- 2.43s, 전진 +1.0m
                          sequence = seq("sword and shield slash (4)_mixamo_com"),
                          blend_in = 0.1, attack_id = "light3", hit_frac = 0.45, window = { 0.55, 0.85 },
                          play_rate = { 1.2, 1.3 } },
        ss_slash2     = { montage = montage("sword and shield slash (2)_mixamo_com"),    -- 3.53s, 전진 +2.3m (속도 보정)
                          sequence = seq("sword and shield slash (2)_mixamo_com"),
                          blend_in = 0.1, attack_id = "light3", hit_frac = 0.45, window = { 0.55, 0.85 },
                          play_rate = { 1.4, 1.6 } },
        gs_slash2     = { montage = montage("great sword slash (2)_mixamo_com"),         -- 3.53s, 전진 +3.0m (속도 보정)
                          sequence = seq("great sword slash (2)_mixamo_com"),
                          blend_in = 0.1, attack_id = "light3", hit_frac = 0.45, window = { 0.55, 0.85 },
                          play_rate = { 1.4, 1.6 } },

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
        air_slash1    = { montage = montage("Barbarian_Melee Attack Downward"),
                          sequence = seq("Barbarian_Melee Attack Downward"),
                          blend_in = 0.1, attack_id = "air1", hit_frac = 0.40, window = { 0.50, 0.85 },
                          play_rate = { 1.95, 2.15 } },
        air_slash2    = { montage = montage("Barbarian_Melee Attack Horizontal"),
                          sequence = seq("Barbarian_Melee Attack Horizontal"),
                          blend_in = 0.1, attack_id = "air2", hit_frac = 0.40, window = { 0.50, 0.85 },
                          play_rate = { 1.95, 2.15 } },

        -- 강공격
        backhand      = { montage = montage("Barbarian_Melee Attack Backhand"), blend_in = 0.2 },

        -- 공중 강공격 — Jump Attack (3.80s, 전진 +2.25m, 긴 클립이라 속도 보정).
        -- RM=0 + ForceRootLock 으로 임포트된 에셋 — force_root_motion 으로 런타임 RM 활성.
        jump_heavy    = { montage = montage("Jump Attack_mixamo_com"),
                          sequence = seq("Jump Attack_mixamo_com"),
                          blend_in = 0.12, attack_id = "jump_heavy", hit_frac = 0.50,
                          play_rate = { 1.25, 1.35 }, force_root_motion = true, 
                          -- 측면 추적 샷 — 도약~착지 임팩트(hit 0.5)까지 옆에서 잡고 복귀
                          camera = { { begin_frac = 0.08, end_frac = 0.62, blend_in = 0.15, blend_out = 0.25,
                                      offset = { 1.0, 4.0, 0.6 }, fov = 60 } } },
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
                          play_rate = 1.05,
                          -- 무쌍기 방점 — 정면 로우앵글 + 레터박스로 임팩트 강조 후 복귀
                          camera = { { begin_frac = 0.05, end_frac = 0.75, blend_in = 0.12, blend_out = 0.3,
                                       offset = { 3.5, -2.0, 0.3 }, fov = 55, look_height = 0.8,
                                       letterbox = 0.12 } } },

        -- ▶ 신규 무쌍기 1단 — 제자리 백플립으로 뒤로 빠짐 (Backflip, 로컬 에셋).
        --   · 발동 시 글로벌 슬로모(OnUltimatePressed)로 "잠시 멈췄다" 시네마틱하게 들어감.
        --   · leap: 도약 프레임에 후방+상방 임펄스 — 제자리 백플립을 실제로 뒤로 빼준다.
        --       up 으로 높이, gravity(<1)로 체공 연장 → 공중에서 2단 강타가 돌게 한다.
        --       체공 ≈ 2·up/(9.8·gravity). 궁극기 종료 시 중력/회전 자동 복원.
        --   · advance: 적당한 시점에 2단(백핸드 강타)으로 조기 cross-fade (안 박히면 몽타주 끝에 폴백).
        --   ※ 에디터에서 직접 UltimateLeap/UltimateAdvance notify 를 저작하면 그게 우선.
        ult_backflip  = { sequence = seq("Backflip_mixamo_com"),
                          blend_in = 0.08, play_rate = 1.0,
                          -- 도입 시네마틱 — 살짝 끌어내린 로우앵글 + 레터박스로 "강한 걸 쓴다" 강조
                          camera = { { begin_frac = 0.0, end_frac = 0.9, blend_in = 0.2, blend_out = 0.25,
                                       offset = { 4.0, -2.0, 0.4 }, fov = 54, look_height = 0.8,
                                       letterbox = 0.14 } },
                          leap    = { trigger_frac = 0.12, back = 6.0, up = 4.5, gravity = 0.3 },
                          advance = { trigger_frac = 0.62 } },

        -- ▶ 신규 무쌍기 2단 — 백핸드 강타 + 전방 진행 충격파(검기). (Standing Backhand, 로컬 에셋)
        --   · hit_frac: 강타 임팩트 1회(캐릭터 위치 musou_slam 판정).
        --   · shockwave.trigger_frac: 검기가 날아가는 타이밍 — origin 만 전진하며 distance/duration
        --     동안 pulses 개의 검기(placeholder) + musou_slam 판정을 순차 발사.
        ult_backhand  = { sequence = seq("Standing Melee Attack Backhand_mixamo_com"),
                          blend_in = 0.15, attack_id = "musou_slam", hit_frac = 0.40,
                          play_rate = 1.0, plant_in_air = true,   -- 강타 시작 시 속도0/중력0 → 공중 제자리 고정
                          -- 검기 사선 프레이밍 — 캐릭터(타겟 facing) 기준 뒤/측면/높이에서 전방 7m 를
                          -- 바라봐 전방으로 날아가는 검기를 화면에 담는다. anchor=character (타겟 사선 추적).
                          --   look_ahead 키우면 더 멀리, offset 으로 카메라 위치(전후/좌우/상하) 조정.
                          camera = { { begin_frac = 0.0, end_frac = 0.85, blend_in = 0.2, blend_out = 0.3,
                                       offset = { -3.0, -4.5, 3.0 }, fov = 60, look_ahead = 7.0, look_height = 0.5,
                                       anchor = "character", letterbox = 0.14 } },
                          -- 검기 발사 타이밍 — 강타 직후. 전방 14m, 0.8s 동안 10발 순차 (타겟 방향으로).
                          -- slash_yaw: 검기 메시 시각 회전 보정(진행 yaw + 이 값). 기존 검기 규칙=90.
                          shockwave = { trigger_frac = 0.42, distance = 14.0, duration = 0.8,
                                        pulses = 10, attack_id = "musou_wave",
                                        slash_speed = 9.0, slash_life = 0.45, slash_yaw = 0 } },

        -- 구르기 (Shift) — 입력 방향 회피. 공격 아님 (attack_id 없음), 전 구간 무적은 C++.
        -- ※ Standing Dive Forward.fbx 를 에디터에서 Barbarian 스켈레톤으로 임포트해야 활성화.
        --   force_root_motion: 임포트 직후 RM 플래그가 꺼져 있어도 런타임에 강제로 켠다.
        roll          = { montage = montage("Standing Dive Forward_mixamo_com"),
                          sequence = seq("Standing Dive Forward_mixamo_com"),
                          blend_in = 0.08, play_rate = 1.35, force_root_motion = true },

        -- 플레이어 피격 리액션 — 좌/우 휘청 (공격 아님). 빠르게 재생해 후딜 최소화
        react_left    = { montage = montage("Barbarian_React Large From Left"),
                          sequence = seq("Barbarian_React Large From Left"),
                          blend_in = 0.08, play_rate = { 1.7, 1.9 } },
        react_right   = { montage = montage("Barbarian_React Large From Right"),
                          sequence = seq("Barbarian_React Large From Right"),
                          blend_in = 0.08, play_rate = { 1.7, 1.9 } },

        -- 발도/납도 (X) — 등 뒤로 뽑기/넣기. 본 스왑 시점은 feedback.weapon.swap_frac
        equip         = { montage = montage("Barbarian_Equip Over Shoulder"),
                          sequence = seq("Barbarian_Equip Over Shoulder"),
                          blend_in = 0.1, play_rate = 1.2 },
        disarm        = { montage = montage("Barbarian_Disarm Over Shoulder"),
                          sequence = seq("Barbarian_Disarm Over Shoulder"),
                          blend_in = 0.1, play_rate = 1.2 },
    },

    chains = {
        -- 좌클릭 콤보 — 진입 컨텍스트별. 이동 중엔 1단이 돌진 베기로 바뀌고 2단부터 합류.
        -- 1/2단은 변주 배열 — 매 콤보 랜덤 (직전 모션 반복 회피). 3단 피니셔는 고정이 연출상 안정적.
        light = {
            -- 전 단수 변주 풀 (2/3단은 idle/moving 공용) — 직전 모션 반복 회피는 C++ 처리
            --idle   = { { "ss_slash", "ss_slash5", "gs_slash" },
            idle   = { { "ss_slash5", "gs_slash" },
                       { "gs_slash4", "ss_slash3", "ss_atk2", "ss_atk3", "gs_slash3", "inward_slash" },
                       { "combo_v3", "combo_v1", "combo_v2", "ss_slash4", "ss_slash2", "gs_slash2" } },
            moving = { { "slide", "ss_lunge" },
                       { "gs_slash4", "ss_slash3", "ss_atk2", "ss_atk3", "gs_slash3", "inward_slash" },
                       { "combo_v3", "combo_v1", "combo_v2", "ss_slash4", "ss_slash2", "gs_slash2" } },

            -- 일반 점프 공격 — 단발 내려찍기 (기존 동작). 행 타임 없음.
            air        = { "jump" },

            -- launcher(branch2 self_launch) 로 떠올랐을 때만 — 공중 저글 3단.
            -- 슬래시 2연타(재띄움으로 저글 유지, 정점 이후 행 타임) → 내려찍기 피니셔로 착지.
            air_juggle = { "air_slash1", "air_slash2", "jump_heavy" },
        },

        -- 우클릭 강공격 — 컨텍스트별 단발
        heavy = {
            idle   = "backhand",
            moving = "spin_high_fwd",
            air    = "jump_heavy",   -- 전용 도약 강타 (저글 중 강공격도 동일 — air_juggle 폴백)
        },

        -- 콤보 분기 피니셔 (□..△) — 인덱스 = 분기 시점 단수. 단수 초과 시 마지막으로 clamp
        branch = { "horizontal", "spin_low", "spin_high" },

        -- 무쌍기 (R, 게이지 가득) — 2단: 백플립(뒤로 빠짐)→백핸드 강타+전방 진행 충격파. 전 구간 무적.
        -- 발동 시 슬로모로 시네마틱 진입. 1단 advance notify 가 2단으로 조기 전환(폴백: 몽타주 끝).
        -- 충격파(검기)는 2단 shockwave.trigger_frac 에서 캐릭터 Tick 이 독립 구동.
        ultimate = { "ult_backflip", "ult_backhand" },

        -- 구르기 (Shift) — 후딜(콤보 윈도우/말미) 캔슬 가능, 전 구간 무적
        dodge = "roll",

        -- 플레이어 피격 리액션 — 좌/우 랜덤 (직전 반복 회피). 평시 피격만, 쿨다운은 feedback
        hit_react = { "react_left", "react_right" },

        -- 발도/납도 (X) — 납도 중 공격 입력도 발도로 변환. 모션 미정의 시 즉시 스왑 폴백
        weapon_draw    = "equip",
        weapon_sheathe = "disarm",
    },

    -- ── 전투 피드백/연출 (AMusouGameMode / AMusouCharacter 소비) ──
    feedback = {
        -- 킬 버스트 — 스윙 1회 판정으로 min_kills 이상 처치 시 글로벌 슬로모 + 강셰이크
        kill_burst = {
            min_kills  = 2,
            slomo_dur  = 0.25,  -- 슬로모 지속 (실시간 초)
            slomo_rate = 0.25,  -- 타임스케일 (0..1, 낮을수록 느려짐)
            shake      = 0.4,   -- 버스트 카메라 셰이크 강도
        },

        -- 공중 콤보 행 타임 — 공중 체인 진행 중 플레이어 중력 배율 (1 = 그대로)
        air_combo = {
            gravity_scale = 0.25,
        },

        -- 무쌍 게이지 — 이 적중(히트) 수를 누적하면 무쌍기(R) 발동 가능 (킬 아님)
        ultimate = {
            hits_to_fill = 40,
        },

        -- 플레이어 피격 리액션 — 최소 간격 (초). 군체 다단 히트 스턴락 방지
        hit_react = {
            cooldown = 3.0,
        },

        -- 발도/납도 — 모션 진행 중 무기 본 스왑(손↔등) 시점 (재생 길이 비율)
        weapon = {
            swap_frac = 0.45,
        },
    },
}
