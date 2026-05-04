local WeaponDefs = {}

WeaponDefs.MainCannon = {
    DisplayName = "머포",
    MaxLevel = 3,
    MaxNormalLevel = 2,
    EvolutionRequiresChest = true,
    Levels = {
        [1] = {
            Damage = 10,
            FireInterval = 0.7,
            ProjectileSpeed = 20,
            Pierce = 0,
            ProjectileScale = 1.0,
            ColliderSize = 1.0,
        },
        [2] = {
            Damage = 12,
            FireInterval = 0.45,
            ProjectileSpeed = 24,
            Pierce = 2,
            ProjectileScale = 1.0,
            ColliderSize = 1.0,
        },
        [3] = {
            Damage = 30,
            FireInterval = 0.5,
            ProjectileSpeed = 18,
            Pierce = 5,
            ProjectileScale = 4.0,
            ColliderSize = 4.0,
        },
    },
}

WeaponDefs.MachineTurret = {
    MaxLevel = 3,
    Levels = {
        [1] = {
            TurretCount = 1,
            FireInterval = 1.0,
            Damage = 3,
        },
        [2] = {
            TurretCount = 2,
            FireInterval = 1.0,
            Damage = 3,
        },
        [3] = {
            TurretCount = 6,
            FireInterval = 1,
            Damage = 3,
        },
    },
}

WeaponDefs.Aura = {
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

return WeaponDefs
