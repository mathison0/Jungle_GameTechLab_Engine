-- player_anim.lua — GameJam 플레이어 (Barbarian) AnimInstance — locomotion 전용
--
--   Root = DefaultSlot ← TopSM (Locomotion ↔ Jump)
--          Locomotion sub-SM: Idle ↔ Walk ↔ Run (속도 기반)
--
-- 사용 방법:
--   1) ACharacter 의 SkeletalMesh 컴포넌트 선택 (Barbarian_SkeletalMesh)
--   2) Animation Mode = Custom + Anim Instance Class = ULuaAnimInstance
--   3) Script File = "Anim/player_anim.lua"
--
-- ※ 공격 입력/몽타주 재생은 C++ AMusouCharacter가 담당:
--   - 입력: SetupInputComponent (좌클릭 콤보 / 우클릭 강공격) — 매 프레임 InputComponent 처리.
--     (lua update()는 Animation Tick LOD 게이트 뒤라 에지 입력이 소실될 수 있어 이관함)
--   - 히트 판정: 몽타주의 AnimNotify_MusouAttack → GameMode 이벤트 파이프라인
--   - 콤보 윈도우: AnimNotifyState_ComboWindow → UComboComponent 직결
--   이 스크립트는 베이스 포즈(locomotion)와 DefaultSlot(몽타주 진입점)만 책임진다.
--
-- Hot-reload: 이 파일 저장만 해도 에디터에서 즉시 반영.

local BARBARIAN_DIR = "Content/Data/GameJam/Barbarian/"

-- ── Locomotion 시퀀스 — 발도(그레이트소드) / 납도(비무장) 2세트 ──
local ARMED_IDLE_PATH = BARBARIAN_DIR .. "great sword idle_mixamo_com.uasset"
local ARMED_WALK_PATH = BARBARIAN_DIR .. "great sword walk_mixamo_com.uasset"
local ARMED_RUN_PATH  = BARBARIAN_DIR .. "great sword run (2)_mixamo_com.uasset"
local ARMED_JUMP_PATH = BARBARIAN_DIR .. "great sword jump_mixamo_com.uasset"

local UNARMED_IDLE_PATH = BARBARIAN_DIR .. "Idle_mixamo_com.uasset"
local UNARMED_WALK_PATH = BARBARIAN_DIR .. "Walking_mixamo_com.uasset"
local UNARMED_RUN_PATH  = BARBARIAN_DIR .. "Running_mixamo_com.uasset"
local UNARMED_JUMP_PATH = BARBARIAN_DIR .. "sword and shield jump_mixamo_com.uasset"

local DEFAULT_SLOT = "DefaultSlot"

-- ── 튜닝 상수 ──
local WALK_THRESHOLD = 0.5    -- 이 속도 초과 = Walk
local RUN_THRESHOLD  = 5.0  -- 이 속도 초과 = Run (캐릭터 이동속도에 맞춰 조절)
local LOCO_BLEND     = 0.2    -- locomotion 상태 전환 블렌드 시간
local ARM_BLEND      = 0.25   -- 발도↔납도 로코모션 전환 블렌드
local JUMP_BLEND_IN  = 0.1
local JUMP_BLEND_OUT = 0.3

-- Idle/Walk/Run sub-SM 생성기 — 발도/납도 세트가 구조 동일.
local function make_locomotion(self, name, idle_path, walk_path, run_path)
    local loco = Anim.create_state_machine(name)
    Anim.sm_add_state(loco, "Idle", Anim.create_sequence_player(idle_path, 1.0, true))
    Anim.sm_add_state(loco, "Walk", Anim.create_sequence_player(walk_path, 1.0, true))
    Anim.sm_add_state(loco, "Run",  Anim.create_sequence_player(run_path,  1.0, true))

    Anim.sm_add_transition(loco, "Idle", "Walk",
        function() return self.Speed >  WALK_THRESHOLD end, LOCO_BLEND)
    Anim.sm_add_transition(loco, "Walk", "Idle",
        function() return self.Speed <= WALK_THRESHOLD end, LOCO_BLEND)
    Anim.sm_add_transition(loco, "Walk", "Run",
        function() return self.Speed >  RUN_THRESHOLD end, LOCO_BLEND)
    Anim.sm_add_transition(loco, "Run", "Walk",
        function() return self.Speed <= RUN_THRESHOLD end, LOCO_BLEND)
    -- 달리다 급정지 시 Walk 거치지 않고 바로 Idle
    Anim.sm_add_transition(loco, "Run", "Idle",
        function() return self.Speed <= WALK_THRESHOLD end, LOCO_BLEND)
    Anim.sm_set_initial_state(loco, "Idle")
    return loco
end

function init(self)
    self.Speed = 0

    -- ── Locomotion 2세트 — C++ 의 SetAnimFlag("WeaponDrawn") 으로 전환 ──
    local loco_armed   = make_locomotion(self, "LocomotionArmed",
        ARMED_IDLE_PATH, ARMED_WALK_PATH, ARMED_RUN_PATH)
    local loco_unarmed = make_locomotion(self, "LocomotionUnarmed",
        UNARMED_IDLE_PATH, UNARMED_WALK_PATH, UNARMED_RUN_PATH)

    -- ── Top SM (Unarmed ↔ Armed, 점프도 무기 상태별 분리) ──
    local top = Anim.create_state_machine("Top")
    Anim.sm_add_state(top, "Unarmed", loco_unarmed)
    Anim.sm_add_state(top, "Armed",   loco_armed)
    Anim.sm_add_state(top, "JumpArmed",   Anim.create_sequence_player(ARMED_JUMP_PATH,   1.0, false))
    Anim.sm_add_state(top, "JumpUnarmed", Anim.create_sequence_player(UNARMED_JUMP_PATH, 1.0, false))

    Anim.sm_add_transition(top, "Unarmed", "Armed",
        function() return Anim.get_flag("WeaponDrawn") end, ARM_BLEND)
    Anim.sm_add_transition(top, "Armed", "Unarmed",
        function() return not Anim.get_flag("WeaponDrawn") end, ARM_BLEND)

    -- 낙하 진입 — 무기 상태에 맞는 점프로. (지상 상태에서만 — 점프 간 상호 전이 방지)
    Anim.sm_add_transition(top, "Unarmed", "JumpUnarmed",
        function() return Anim.is_owner_falling() end, JUMP_BLEND_IN)
    Anim.sm_add_transition(top, "Armed", "JumpArmed",
        function() return Anim.is_owner_falling() end, JUMP_BLEND_IN)

    -- 착지 복귀 — 그 시점 무기 상태로 (공중 발도 등 상태 변화 케이스 포함)
    Anim.sm_add_transition(top, "JumpArmed", "Armed",
        function() return not Anim.is_owner_falling() and Anim.get_flag("WeaponDrawn") end, JUMP_BLEND_OUT)
    Anim.sm_add_transition(top, "JumpArmed", "Unarmed",
        function() return not Anim.is_owner_falling() and not Anim.get_flag("WeaponDrawn") end, JUMP_BLEND_OUT)
    Anim.sm_add_transition(top, "JumpUnarmed", "Armed",
        function() return not Anim.is_owner_falling() and Anim.get_flag("WeaponDrawn") end, JUMP_BLEND_OUT)
    Anim.sm_add_transition(top, "JumpUnarmed", "Unarmed",
        function() return not Anim.is_owner_falling() and not Anim.get_flag("WeaponDrawn") end, JUMP_BLEND_OUT)

    -- 시작은 납도 (C++ 기본값과 일치 — bWeaponDrawn = false)
    Anim.sm_set_initial_state(top, "Unarmed")

    -- ── DefaultSlot — 풀바디 montage 진입점 (C++에서 재생하는 공격 몽타주가 여기로) ──
    -- 상반신 분리가 필요해지면 yui_character.lua 의 LayeredBlendPerBone 패턴 참고.
    local default_slot = Anim.create_slot(DEFAULT_SLOT, top)

    Anim.set_root_node(default_slot)
end

function update(self, dt)
    self.Speed = Anim.get_owner_speed()
end

function on_notify(self, name)
    -- 이름 기반 notify의 lua 연동용 큐 (클래스 notify는 C++에서 직접 처리됨).
    _G.MusouAnimEvents = _G.MusouAnimEvents or {}
    table.insert(_G.MusouAnimEvents, name)
    print("[PlayerAnim] notify: " .. name)
end

-- =============================================================================
-- 사용 가능한 Barbarian 애니메이션 (Content/Data/GameJam/Barbarian/, 임포트 완료)
-- =============================================================================
-- [이동]   Barbarian_Idle / Walk Forward / Run Forward / Run Back / Jump_mixamo_com
-- [공격]   Melee Attack Horizontal / Backhand / Downward / 360 High / 360 Low
--          Melee Attack Kick Ver. 2 / Melee Run Jump Attack
-- [콤보]   Melee Combo Attack Ver. 1 / Ver. 2 / Ver. 3
-- [피격]   Block React Large / React Large From Left / From Right
-- [장비]   Equip Over Shoulder / Disarm Over Shoulder
-- [도발]   Taunt Battlecry / Taunt Chest Thump
-- =============================================================================
