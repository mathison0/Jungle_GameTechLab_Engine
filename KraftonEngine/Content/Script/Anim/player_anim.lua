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

-- ── Locomotion 시퀀스 ──
local IDLE_PATH = BARBARIAN_DIR .. "Barbarian_Idle.uasset"
local WALK_PATH = BARBARIAN_DIR .. "Barbarian_Walk Forward.uasset"
local RUN_PATH  = BARBARIAN_DIR .. "Barbarian_Run Forward.uasset"
local JUMP_PATH = BARBARIAN_DIR .. "Barbarian_Jump_mixamo_com.uasset"

local DEFAULT_SLOT = "DefaultSlot"

-- ── 튜닝 상수 ──
local WALK_THRESHOLD = 0.5    -- 이 속도 초과 = Walk
local RUN_THRESHOLD  = 350.0  -- 이 속도 초과 = Run (캐릭터 이동속도에 맞춰 조절)
local LOCO_BLEND     = 0.2    -- locomotion 상태 전환 블렌드 시간
local JUMP_BLEND_IN  = 0.1
local JUMP_BLEND_OUT = 0.3

function init(self)
    self.Speed = 0

    -- ── Locomotion sub-SM (Idle ↔ Walk ↔ Run) ──
    local loco = Anim.create_state_machine("Locomotion")
    Anim.sm_add_state(loco, "Idle", Anim.create_sequence_player(IDLE_PATH, 1.0, true))
    Anim.sm_add_state(loco, "Walk", Anim.create_sequence_player(WALK_PATH, 1.0, true))
    Anim.sm_add_state(loco, "Run",  Anim.create_sequence_player(RUN_PATH,  1.0, true))

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

    -- ── Top SM (Locomotion ↔ Jump) ──
    local top = Anim.create_state_machine("Top")
    Anim.sm_add_state(top, "Locomotion", loco)
    Anim.sm_add_state(top, "Jump", Anim.create_sequence_player(JUMP_PATH, 1.0, false))
    Anim.sm_add_transition(top, "AnyState", "Jump",
        function() return Anim.is_owner_falling() end, JUMP_BLEND_IN)
    Anim.sm_add_transition(top, "Jump", "Locomotion",
        function() return not Anim.is_owner_falling() end, JUMP_BLEND_OUT)
    Anim.sm_set_initial_state(top, "Locomotion")

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
