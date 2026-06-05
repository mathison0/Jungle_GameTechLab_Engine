-- player_anim.lua — GameJam 프로토타입 플레이어 (Barbarian) AnimInstance
--
--   Root = DefaultSlot ← TopSM (Locomotion ↔ Jump)
--          Locomotion sub-SM: Idle ↔ Walk ↔ Run (속도 기반)
--
-- 사용 방법:
--   1) ACharacter 의 SkeletalMesh 컴포넌트 선택 (Barbarian_SkeletalMesh)
--   2) Animation Mode = Custom + Anim Instance Class = ULuaAnimInstance
--   3) Script File = "Anim/player_anim.lua"
--
-- 입력 (프로토타입):
--   좌클릭 = Attack1 / 우클릭 = Attack2
--   1, 2, 3 = 스킬 1/2/3 (몽타주 경로 채우면 동작)
--
-- TODO(확장 계획):
--   좌클릭 콤보 어택  → MONTAGES.combo 에 섹션 나눈 몽타주 연결 후
--                       on_notify 콤보 윈도우 + Anim.jump_to_section 으로 구현
--   우클릭 강공격     → MONTAGES.heavy
--   몽타주는 빈 문자열이면 무시되므로 경로만 채워 넣으면 된다.
--
-- Hot-reload: 이 파일 저장만 해도 에디터에서 즉시 반영.

local BARBARIAN_DIR = "Content/Data/GameJam/Barbarian/"

-- ── Locomotion 시퀀스 (임포트 완료된 uasset) ──
local IDLE_PATH = BARBARIAN_DIR .. "Barbarian_Idle.uasset"
local WALK_PATH = BARBARIAN_DIR .. "Barbarian_Walk Forward.uasset"
local RUN_PATH  = BARBARIAN_DIR .. "Barbarian_Run Forward.uasset"
local JUMP_PATH = BARBARIAN_DIR .. "Barbarian_Jump_mixamo_com.uasset"

-- ── 몽타주 경로 — 직접 채워넣기 (빈 문자열 = 미설정, 입력 무시) ──
-- 시퀀스 후보는 파일 하단 "사용 가능한 Barbarian 애니메이션" 주석 참고.
local MONTAGES = {
    attack1 = "Content/Montages/Barbarian_Melee Attack 360 High_Montage.uasset",
    attack2 = "Content/Montages/Barbarian_Melee Attack BackHand_Montage.uasset",
    heavy   = "",   -- (추후) 우클릭 강공격 — Melee Attack Downward / 360 High 추천
    skill1  = "",   -- (추후) 스킬1 — Melee Attack 360 Low 등
    skill2  = "",   -- (추후) 스킬2 — Melee Run Jump Attack 등
    skill3  = "",   -- (추후) 스킬3 — Taunt Battlecry (버프류) 등
}

-- ── 좌클릭 콤보 체인 (UComboComponent 연동) ──
-- 단계 상태/입력 버퍼/윈도우는 C++ UComboComponent가 관리하고,
-- 여기서는 단계 → 몽타주 매핑과 재생만 담당한다.
-- 각 몽타주에는 AnimNotify_MusouAttack(comboN) + AnimNotifyState_ComboWindow 배치 필요.
local COMBO_MONTAGES = {
    "Content/Montages/Barbarian_Melee Combo Attack Ver. 1_Montage.uasset",
    "Content/Montages/Barbarian_Melee Combo Attack Ver. 2_Montage.uasset",
    "Content/Montages/Barbarian_Melee Combo Attack Ver. 3_Montage.uasset",
}
local COMBO_PLAY_RATE = 1.0
local COMBO_BLEND_IN = 0.1

local DEFAULT_SLOT = "DefaultSlot"

-- ── 튜닝 상수 ──
local WALK_THRESHOLD     = 0.5    -- 이 속도 초과 = Walk
local RUN_THRESHOLD      = 350.0  -- 이 속도 초과 = Run (캐릭터 이동속도에 맞춰 조절)
local LOCO_BLEND         = 0.2    -- locomotion 상태 전환 블렌드 시간
local JUMP_BLEND_IN      = 0.1
local JUMP_BLEND_OUT     = 0.3
local MONTAGE_PLAY_RATE  = 1.0
local MONTAGE_BLEND_IN   = 0.2

local KEY_1 = string.byte("1")
local KEY_2 = string.byte("2")
local KEY_3 = string.byte("3")

-- ── 몽타주 액션 공통 처리 — 경로 미설정/공중/재생 중이면 무시 ──
local function play_montage_action(name)
    local path = MONTAGES[name]
    if path == nil or path == "" then
        print("[PlayerAnim] montage 미설정: " .. name)
        return
    end
    if Anim.is_owner_falling() or Anim.is_montage_playing(DEFAULT_SLOT) then
        return
    end
    Anim.play_montage(path, nil, MONTAGE_PLAY_RATE, MONTAGE_BLEND_IN, nil)
end

-- ── 콤보 헬퍼 ──
-- ComboComponent는 캐릭터 구성 순서상 anim init 이후에 붙을 수 있어 lazy 캐시.
local function get_combo(self)
    if self.Combo == nil then
        local owner = Anim.get_owner()
        if owner ~= nil then
            self.Combo = owner:GetComboComponent()
        end
    end
    return self.Combo
end

local function play_combo_step(step)
    local path = COMBO_MONTAGES[step]
    if path == nil then
        return
    end
    Anim.play_montage(path, nil, COMBO_PLAY_RATE, COMBO_BLEND_IN, nil)
end

function init(self)
    self.Speed = 0
    self.Combo = nil

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

    -- ── DefaultSlot — 풀바디 montage 진입점 (attack/skill 몽타주가 여기서 재생) ──
    -- 상반신 분리가 필요해지면 yui_character.lua 의 LayeredBlendPerBone 패턴 참고.
    local default_slot = Anim.create_slot(DEFAULT_SLOT, top)

    Anim.set_root_node(default_slot)
end

function update(self, dt)
    self.Speed = Anim.get_owner_speed()

    local combo = get_combo(self)

    -- ── 콤보 리셋 — 체인 끊김(윈도우 내 미입력)/완주/공중 ──
    -- 입력 처리보다 먼저: 같은 프레임에 리셋 후 새 콤보 시작이 가능하도록.
    if combo ~= nil and combo:IsComboActive() then
        if Anim.is_owner_falling() or not Anim.is_montage_playing(DEFAULT_SLOT) then
            combo:ResetCombo()
        end
    end

    -- ── 좌클릭 → 콤보 체인 (ComboComponent 없으면 단발 attack1 폴백) ──
    if Anim.is_left_mouse_pressed() then
        if combo ~= nil then
            -- 다른 몽타주(강공격/스킬) 재생 중엔 새 콤보 시작 금지.
            -- 콤보 진행 중이면 TryAttack이 내부에서 다음 단계 입력으로 버퍼링.
            local bOtherMontagePlaying = not combo:IsComboActive() and Anim.is_montage_playing(DEFAULT_SLOT)
            if not Anim.is_owner_falling() and not bOtherMontagePlaying then
                if combo:TryAttack() then
                    play_combo_step(1)
                end
            end
        else
            play_montage_action("attack1")
        end
    end

    -- ── 콤보 단계 전진 — 윈도우 내 예약 입력 소비 ──
    if combo ~= nil and combo:ConsumeQueuedAdvance() then
        play_combo_step(combo:GetComboStep())
    end

    -- 우클릭 → Attack2 (추후 강공격으로 교체 예정)
    if Anim.is_right_mouse_pressed() then
        play_montage_action("attack2")
    end

    -- 스킬 1/2/3
    if Anim.is_key_pressed(KEY_1) then play_montage_action("skill1") end
    if Anim.is_key_pressed(KEY_2) then play_montage_action("skill2") end
    if Anim.is_key_pressed(KEY_3) then play_montage_action("skill3") end
end

function on_notify(self, name)
    -- ※ 공격 히트는 C++ AnimNotify_MusouAttack(클래스 notify)이 GameMode로 직접
    --   브로드캐스트하므로 여기를 거치지 않는다. 이 큐는 콤보 윈도우 등
    --   이름 기반 notify("ComboWindowOpen"/"ComboEnd")의 lua 연동용.
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
-- ※ Block Idle 은 fbx 만 있고 uasset 미임포트 상태 (필요 시 임포트)
-- =============================================================================
