local MONTAGE_DIR = "Content/Montages/"

local function montage(name) return MONTAGE_DIR .. name .. "_Montage.uasset" end

return {
    bosses = {
        knight_boss = {
            max_hp = 1200,
            attack_power = 18,
            move_speed = 3.2,
            stop_distance = 2.8,
            mesh = "Content/Data/GameJam/Warrior/SK_Warrior_SkeletalMesh.uasset",
            anim_script = "Anim/boss_knight_anim.lua",
            idle_montage = montage("sword and shield idle (4)_mixamo_com"),
            run_montage = montage("sword and shield run_mixamo_com"),

            patterns = {
                {
                    id = "slash",
                    attack_id = "boss_slash",
                    montage = montage("sword and shield slash_mixamo_com"),
                    min_range = 0.0,
                    max_range = 4.5,
                    cooldown = 2.0,
                    telegraph = 0.45,
                    attack_time = 0.15,
                    recovery = 0.75,
                    weight = 5,
                    hp = { 0.0, 1.0 },
                },
                {
                    id = "spin",
                    attack_id = "boss_spin",
                    min_range = 0.0,
                    max_range = 6.0,
                    cooldown = 5.5,
                    telegraph = 0.9,
                    attack_time = 0.1,
                    recovery = 1.2,
                    weight = 2,
                    hp = { 0.0, 0.65 },
                },
                {
                    id = "dash_slash",
                    attack_id = "boss_dash_slash",
                    min_range = 4.0,
                    max_range = 9.0,
                    cooldown = 4.0,
                    recovery = 0.9,
                    weight = 3,
                    hp = { 0.0, 1.0 },
                    steps = {
                        { type = "face_target", time = 0.0 },
                        { type = "play_montage", time = 0.0, montage = montage("sword and shield slash_mixamo_com") },
                        { type = "dash", time = 0.20, duration = 0.35, speed = 11.0 },
                        { type = "attack", time = 0.38, attack_id = "boss_dash_slash" },
                    },
                }
            },
        },

        mage_boss = {
            max_hp = 900,
            attack_power = 14,
            move_speed = 2.6,
            stop_distance = 7.0,
            mesh = "Content/Data/GameJam/Knight/SK_Knight_SkeletalMesh.uasset",

            patterns = {
                {
                    id = "bolt",
                    attack_id = "boss_bolt",
                    min_range = 4.0,
                    max_range = 12.0,
                    cooldown = 1.8,
                    telegraph = 0.35,
                    attack_time = 0.1,
                    recovery = 0.6,
                    weight = 5,
                    hp = { 0.0, 1.0 },
                },
                {
                    id = "nova",
                    attack_id = "boss_nova",
                    min_range = 0.0,
                    max_range = 5.5,
                    cooldown = 6.0,
                    telegraph = 0.8,
                    attack_time = 0.1,
                    recovery = 1.0,
                    weight = 3,
                    hp = { 0.0, 0.8 },
                },
            },
        },
    },
}
