-- Knight boss locomotion anim graph.
-- Base pose is driven by owner CharacterMovement speed, while attack montages
-- are layered through DefaultSlot from C++ boss pattern logic.

local WARRIOR_DIR = "Content/Data/GameJam/Warrior/"

local IDLE_PATH = WARRIOR_DIR .. "sword and shield idle (4)_mixamo_com.uasset"
local RUN_PATH  = WARRIOR_DIR .. "sword and shield run_mixamo_com.uasset"
local DEATH_PATH = WARRIOR_DIR .. "sword and shield death_mixamo_com.uasset"

local DEFAULT_SLOT = "DefaultSlot"
local RUN_THRESHOLD = 0.5
local LOCO_BLEND = 0.2

function init(self)
    self.Speed = 0

    local loco = Anim.create_state_machine("BossLocomotion")
    Anim.sm_add_state(loco, "Idle", Anim.create_sequence_player(IDLE_PATH, 1.0, true))
    Anim.sm_add_state(loco, "Run",  Anim.create_sequence_player(RUN_PATH,  1.0, true))
    Anim.sm_add_state(loco, "Death", Anim.create_sequence_player(DEATH_PATH, 1.0, false))

    Anim.sm_add_transition(loco, "AnyState", "Death",
        function()
            return is_dead()
        end, 0.1)

    Anim.sm_add_transition(loco, "Idle", "Run",
        function() return self.Speed > RUN_THRESHOLD end, LOCO_BLEND)
    Anim.sm_add_transition(loco, "Run", "Idle",
        function() return self.Speed <= RUN_THRESHOLD and not is_dead() end, LOCO_BLEND)
    Anim.sm_set_initial_state(loco, "Idle")

    local default_slot = Anim.create_slot(DEFAULT_SLOT, loco)
    Anim.set_root_node(default_slot)
end

function update(self, dt)
    self.Speed = Anim.get_owner_speed()
end

function is_dead()
    local owner = Anim.get_owner()
    if not owner then
        return false
    end

    local battle = owner:GetBattleComponent()
    return battle and battle:IsDead()
end
