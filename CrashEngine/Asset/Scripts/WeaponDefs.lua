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
            FireInterval = 0.7,
            ProjectileSpeed = 40,
            Pierce = 0,
            ProjectileScale = 0.2,
            ColliderSize = 1.0,
        },
        [2] = {
            Damage = 50,
            FireInterval = 0.35,
            ProjectileSpeed = 40,
            Pierce = 2,
            ProjectileScale = 0.4,
            ColliderSize = 1.0,
        },
        [3] = {
            Damage = 50,
            FireInterval = 0.175,
            ProjectileSpeed = 50,
            Pierce = 5,
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
            Damage = 3,
            FireInterval = 1.0,
            TurretCount = 1,
            Range = 10000.0,
            TargetRefreshInterval = 0.2,
        },
        [2] = {
            Damage = 3,
            FireInterval = 1.0,
            TurretCount = 2,
            Range = 10000.0,
            TargetRefreshInterval = 0.2,
        },
        [3] = {
            Damage = 4,
            FireInterval = 0.35,
            TurretCount = 4,
            Range = 10000.0,
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
