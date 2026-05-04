local WeaponDefs = {}

WeaponDefs.MainCannon = {
    DisplayName = "Main Cannon",
    AttackType = "LinearProjectile",
    MaxLevel = 3,
    MaxNormalLevel = 2,
    EvolutionRequiresChest = true,
    Levels = {
        [1] = {
            Damage = 50,
            FireInterval = 1.0,
            ProjectileSpeed = 150,
            Pierce = 1,
            ProjectileScale = 0.2,
            ColliderSize = 1.0,
        },
        [2] = {
            Damage = 50,
            FireInterval = 0.7,
            ProjectileSpeed = 150,
            Pierce = 5,
            ProjectileScale = 0.4,
            ColliderSize = 1.0,
        },
        [3] = {
            Damage = 50,
            FireInterval = 0.4,
            ProjectileSpeed = 150,
            Pierce = 999,
            ProjectileScale = 0.6,
            ColliderSize = 4.0,
        },
    },
}

WeaponDefs.MachineTurret = {
    DisplayName = "Machine Turret",
    AttackType = "InstantTargetDamage",
    MaxLevel = 3,
    Levels = {
        [1] = {
            Damage = 10,
            FireInterval = 0.5,
            TurretCount = 1,
            RapidCount = 3,
            RapidInterval = 0.1,
            Range = 10.0,
            TargetRefreshInterval = 0.2,
        },
        [2] = {
            Damage = 10,
            FireInterval = 0.5,
            TurretCount = 2,
            RapidCount = 4,
            RapidInterval = 0.1,
            Range = 10.0,
            TargetRefreshInterval = 0.2,
        },
        [3] = {
            Damage = 22,
            FireInterval = 0.5,
            TurretCount = 4,
            RapidCount = 5,
            RapidInterval = 0.1,
            Range = 10.0,
            TargetRefreshInterval = 0.15,
        },
    },
}

WeaponDefs.Aura = {
    DisplayName = "Aura",
    AttackType = "AreaTickDamage",
    MaxLevel = 3,
    Levels = {
        [1] = {
            Radius = 3.0,
            Damage = 2,
            TickInterval = 1.0,
        },
        [2] = {
            Radius = 4.0,
            Damage = 4,
            TickInterval = 0.8,
        },
        [3] = {
            Radius = 5.5,
            Damage = 7,
            TickInterval = 0.5,
        },
    },
}

WeaponDefs.HomingMissile = {
    DisplayName = "Homing Missile",
    AttackType = "HomingMissile",
    MaxLevel = 3,
    Levels = {
        [1] = {
            Damage = 30,
            FireInterval = 2.5,
            ProjectileSpeed = 12,
            TurnSpeed = 120,
            ImpactRadius = 3.0,
            ColliderSize = 0.8,
            ProjectileScale = 1.0,
            Range = 10000.0,
        },
        [2] = {
            Damage = 40,
            FireInterval = 2.2,
            ProjectileSpeed = 13,
            TurnSpeed = 140,
            ImpactRadius = 3.2,
            ColliderSize = 0.8,
            ProjectileScale = 1.0,
            Range = 10000.0,
        },
        [3] = {
            Damage = 55,
            FireInterval = 1.8,
            ProjectileSpeed = 15,
            TurnSpeed = 160,
            ImpactRadius = 3.5,
            ColliderSize = 0.9,
            ProjectileScale = 1.15,
            Range = 10000.0,
        },
    },
}

WeaponDefs.SatelliteBeam = {
    DisplayName = "Satellite Beam",
    AttackType = "InstantTargetDamage",
    MaxLevel = 3,
    Levels = {
        [1] = {
            Damage = 50,
            FireInterval = 4.0,
            Range = 10000.0,
            TargetCount = 1,
        },
        [2] = {
            Damage = 70,
            FireInterval = 3.5,
            Range = 10000.0,
            TargetCount = 1,
        },
        [3] = {
            Damage = 90,
            FireInterval = 3.0,
            Range = 10000.0,
            TargetCount = 1,
        },
    },
}

return WeaponDefs
